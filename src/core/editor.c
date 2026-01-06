/**
 * @file editor.c
 * @brief 編輯器核心模組實作
 * 
 * 實作類 Vim 編輯器的核心功能，包含緩衝區管理、
 * 按鍵處理、模式切換、畫面更新等。
 */

#include "editor.h"
#include "command.h"
#include "vim_ops.h"
#include "buffer_ops.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include "../ui/input.h"
#include "../ui/screen.h"
#include "../ui/colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

/* ============================================================================
 * 私有常數定義
 * ============================================================================ */

/** @brief 最大可開啟的緩衝區數量 */
#define MAX_BUFFERS 16

/** @brief 緩衝區陣列的初始容量 */
#define INITIAL_BUFFER_CAPACITY 4

/* ============================================================================
 * 編輯器生命週期管理實作
 * ============================================================================ */

editor_t *editor_create(void) {
    /* 配置編輯器主結構 */
    editor_t *editor = (editor_t *)safe_malloc(sizeof(editor_t));
    if (editor == NULL) {
        return NULL;
    }
    
    /* 配置緩衝區指標陣列 */
    editor->buffers = (buffer_t **)safe_malloc(sizeof(buffer_t *) * INITIAL_BUFFER_CAPACITY);
    if (editor->buffers == NULL) {
        safe_free(editor);
        return NULL;
    }
    
    /* 初始化所有欄位 */
    editor->buffer_count = 0;
    editor->current_buffer = 0;
    editor->cursor_row = 0;
    editor->cursor_col = 0;
    editor->mode = MODE_NORMAL;
    editor->first_line = 0;
    editor->command_buffer = NULL;
    editor->running = true;
    editor->repeat_count = 0;
    editor->last_operation = 0;
    
    /* 初始化 Vim 操作上下文 */
    editor->vim_ctx = (vim_context_t *)safe_malloc(sizeof(vim_context_t));
    if (editor->vim_ctx != NULL) {
        vim_init_context(editor->vim_ctx);
    }
    
    return editor;
}

void editor_destroy(editor_t *editor) {
    if (editor == NULL) {
        return;
    }
    
    /* 銷毀所有開啟的緩衝區 */
    for (size_t i = 0; i < editor->buffer_count; i++) {
        if (editor->buffers[i] != NULL) {
            buffer_destroy(editor->buffers[i]);
        }
    }
    
    /* 清理 Vim 上下文 */
    if (editor->vim_ctx != NULL) {
        vim_cleanup_context(editor->vim_ctx);
        safe_free(editor->vim_ctx);
    }
    
    /* 釋放其他資源 */
    safe_free(editor->buffers);
    safe_free(editor->command_buffer);
    safe_free(editor);
}

/* ============================================================================
 * 緩衝區管理實作
 * ============================================================================ */

bool editor_open_file(editor_t *editor, const char *filename) {
    /* 參數驗證 */
    if (editor == NULL || filename == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return false;
    }
    
    /* 檢查檔案是否已開啟（避免重複開啟） */
    for (size_t i = 0; i < editor->buffer_count; i++) {
        if (editor->buffers[i] != NULL && 
            editor->buffers[i]->filename != NULL &&
            strcmp(editor->buffers[i]->filename, filename) == 0) {
            /* 已開啟，直接切換到該緩衝區 */
            editor->current_buffer = i;
            return true;
        }
    }
    
    /* 建立新緩衝區 */
    buffer_t *buf = buffer_create(filename);
    if (buf == NULL) {
        return false;
    }
    
    /* 嘗試載入檔案內容（檔案不存在時會建立空緩衝區） */
    if (!buffer_load_from_file(buf, filename)) {
        error_clear();  /* 清除「檔案不存在」的錯誤，視為新檔案 */
    }
    
    /* 檢查緩衝區數量上限 */
    if (editor->buffer_count >= MAX_BUFFERS) {
        buffer_destroy(buf);
        error_set(ERR_INVALID_INPUT, "緩衝區數量已達上限");
        return false;
    }
    
    /* 加入編輯器並切換到新緩衝區 */
    editor->buffers[editor->buffer_count++] = buf;
    editor->current_buffer = editor->buffer_count - 1;
    editor->cursor_row = 0;
    editor->cursor_col = 0;
    editor->first_line = 0;
    
    return true;
}

bool editor_close_buffer(editor_t *editor) {
    /* 參數驗證 */
    if (editor == NULL || editor->buffer_count == 0) {
        return false;
    }
    
    size_t idx = editor->current_buffer;
    
    /* 
     * TODO: 應檢查未儲存的修改並提示使用者
     * 目前暫時直接關閉
     */
    if (editor->buffers[idx] != NULL && buffer_is_modified(editor->buffers[idx])) {
        /* 有未儲存的修改 */
    }
    
    /* 銷毀緩衝區 */
    buffer_destroy(editor->buffers[idx]);
    
    /* 將後面的緩衝區向前移動填補空缺 */
    for (size_t i = idx; i < editor->buffer_count - 1; i++) {
        editor->buffers[i] = editor->buffers[i + 1];
    }
    
    editor->buffer_count--;
    
    /* 調整目前緩衝區索引 */
    if (editor->current_buffer >= editor->buffer_count && editor->buffer_count > 0) {
        editor->current_buffer = editor->buffer_count - 1;
    }
    
    /* 若無緩衝區則結束編輯器 */
    if (editor->buffer_count == 0) {
        editor->running = false;
    }
    
    return true;
}

bool editor_switch_buffer(editor_t *editor, size_t index) {
    /* 參數驗證 */
    if (editor == NULL || index >= editor->buffer_count) {
        return false;
    }
    
    /* 切換並重設游標位置 */
    editor->current_buffer = index;
    editor->cursor_row = 0;
    editor->cursor_col = 0;
    editor->first_line = 0;
    
    return true;
}

/* ============================================================================
 * 輸入處理實作
 * ============================================================================ */

/**
 * @brief 處理 Normal 模式的按鍵
 * @param editor 編輯器實例
 * @param buf 目前的緩衝區
 * @param key 按鍵輸入
 */
static void handle_normal_mode(editor_t *editor, buffer_t *buf, key_input_t *key) {
    switch (key->key) {
        case 'i':
        case 'a':
            /* 進入插入模式 */
            editor_set_mode(editor, MODE_INSERT);
            break;
            
        case 'v':
            /* 進入可視模式 */
            editor_set_mode(editor, MODE_VISUAL);
            break;
            
        case ':':
            /* 進入命令模式 */
            editor_set_mode(editor, MODE_COMMAND);
            safe_free(editor->command_buffer);
            editor->command_buffer = safe_strdup("");
            break;
            
        case 'h':
            /* 游標左移 */
            if (editor->cursor_col > 0) {
                editor->cursor_col--;
            }
            break;
            
        case 'l':
            /* 游標右移 */
            {
                line_t *line = buffer_get_line(buf, editor->cursor_row);
                if (line != NULL && editor->cursor_col < line->length) {
                    editor->cursor_col++;
                }
            }
            break;
            
        case 'j':
            /* 游標下移 */
            if (editor->cursor_row < buf->line_count - 1) {
                editor->cursor_row++;
                /* 調整欄位以避免超出新行的長度 */
                line_t *line = buffer_get_line(buf, editor->cursor_row);
                if (line != NULL && editor->cursor_col > line->length) {
                    editor->cursor_col = line->length;
                }
            }
            break;
            
        case 'k':
            /* 游標上移 */
            if (editor->cursor_row > 0) {
                editor->cursor_row--;
                /* 調整欄位以避免超出新行的長度 */
                line_t *line = buffer_get_line(buf, editor->cursor_row);
                if (line != NULL && editor->cursor_col > line->length) {
                    editor->cursor_col = line->length;
                }
            }
            break;
            
        case 'x':
            /* 刪除游標處的字元 */
            buffer_delete_char(buf, editor->cursor_row, editor->cursor_col);
            break;
            
        case 'd':
            /* dd: 刪除目前行（簡化實作，需要雙擊偵測） */
            buffer_delete_line(buf, editor->cursor_row);
            if (editor->cursor_row >= buf->line_count) {
                editor->cursor_row = buf->line_count - 1;
            }
            break;
    }
}

/**
 * @brief 處理 Insert 模式的按鍵
 * @param editor 編輯器實例
 * @param buf 目前的緩衝區
 * @param key 按鍵輸入
 */
static void handle_insert_mode(editor_t *editor, buffer_t *buf, key_input_t *key) {
    if (key->key == '\x1b') {
        /* Escape: 返回 Normal 模式 */
        editor_set_mode(editor, MODE_NORMAL);
    } else if (key->key == '\b' || key->key == '\x7f') {
        /* Backspace: 刪除前一個字元 */
        if (editor->cursor_col > 0) {
            editor->cursor_col--;
            buffer_delete_char(buf, editor->cursor_row, editor->cursor_col);
        } else if (editor->cursor_row > 0) {
            /* 游標在行首，合併到上一行 */
            line_t *prev_line = buffer_get_line(buf, editor->cursor_row - 1);
            if (prev_line != NULL) {
                size_t prev_len = prev_line->length;
                editor->cursor_row--;
                editor->cursor_col = prev_len;
                /* TODO: 實作行合併功能 */
            }
        }
    } else if (key->key == '\n' || key->key == '\r') {
        /* Enter: 插入新行 */
        buffer_insert_line(buf, editor->cursor_row + 1, "");
        editor->cursor_row++;
        editor->cursor_col = 0;
    } else if (key->key >= 32 && key->key < 127) {
        /* 可列印字元: 插入到游標位置 */
        buffer_insert_char(buf, editor->cursor_row, editor->cursor_col, key->key);
        editor->cursor_col++;
    }
}

/**
 * @brief 處理 Command 模式的按鍵
 * @param editor 編輯器實例
 * @param key 按鍵輸入
 */
static void handle_command_mode(editor_t *editor, key_input_t *key) {
    if (key->key == '\n' || key->key == '\r') {
        /* Enter: 執行命令 */
        if (editor->command_buffer != NULL && strlen(editor->command_buffer) > 0) {
            command_t *cmd = command_parse(editor->command_buffer);
            if (cmd != NULL) {
                switch (cmd->type) {
                    case CMD_QUIT:
                        /* :q - 退出（檢查未儲存的修改） */
                        if (editor->buffer_count > 0) {
                            buffer_t *buf = editor->buffers[editor->current_buffer];
                            if (buf != NULL && buffer_is_modified(buf)) {
                                screen_show_status("有未儲存的修改。使用 :q! 強制退出或 :w 儲存", true);
                            } else {
                                editor->running = false;
                            }
                        } else {
                            editor->running = false;
                        }
                        if (editor->running) {
                            safe_free(editor->command_buffer);
                            editor->command_buffer = safe_strdup("");
                        } else {
                            safe_free(editor->command_buffer);
                            editor->command_buffer = NULL;
                            editor_set_mode(editor, MODE_NORMAL);
                        }
                        break;
                        
                    case CMD_QUIT_FORCE:
                        /* :q! - 強制退出 */
                        safe_free(editor->command_buffer);
                        editor->command_buffer = NULL;
                        editor_set_mode(editor, MODE_NORMAL);
                        editor->running = false;
                        break;
                        
                    case CMD_WRITE:
                        /* :w - 儲存 */
                        if (cmd->arg1 != NULL) {
                            editor_save_as(editor, cmd->arg1);
                        } else {
                            editor_save(editor);
                        }
                        screen_show_status("檔案已儲存", false);
                        safe_free(editor->command_buffer);
                        editor->command_buffer = NULL;
                        editor_set_mode(editor, MODE_NORMAL);
                        break;
                        
                    case CMD_WRITE_QUIT:
                        /* :wq - 儲存並退出 */
                        editor_save(editor);
                        safe_free(editor->command_buffer);
                        editor->command_buffer = NULL;
                        editor_set_mode(editor, MODE_NORMAL);
                        editor->running = false;
                        break;
                        
                    case CMD_EDIT:
                        /* :e <filename> - 開啟檔案 */
                        if (cmd->arg1 != NULL) {
                            editor_open_file(editor, cmd->arg1);
                            editor->cursor_row = 0;
                            editor->cursor_col = 0;
                            editor->first_line = 0;
                        }
                        safe_free(editor->command_buffer);
                        editor->command_buffer = NULL;
                        editor_set_mode(editor, MODE_NORMAL);
                        break;
                        
                    case CMD_BUFFER:
                        /* :b <n> - 切換緩衝區 */
                        if (cmd->arg1 != NULL) {
                            int buf_num = atoi(cmd->arg1);
                            if (buf_num > 0 && (size_t)buf_num <= editor->buffer_count) {
                                editor_switch_buffer(editor, buf_num - 1);
                            }
                        }
                        safe_free(editor->command_buffer);
                        editor->command_buffer = NULL;
                        editor_set_mode(editor, MODE_NORMAL);
                        break;
                        
                    case CMD_SUBSTITUTE:
                        /* :s/old/new/ - 搜尋替換 */
                        if (cmd->arg1 != NULL && cmd->arg2 != NULL) {
                            screen_show_status("搜尋替換功能待實作", false);
                        }
                        safe_free(editor->command_buffer);
                        editor->command_buffer = NULL;
                        editor_set_mode(editor, MODE_NORMAL);
                        break;
                        
                    case CMD_SEARCH:
                        /* /pattern - 搜尋 */
                        if (cmd->arg1 != NULL) {
                            screen_show_status("搜尋功能待實作", false);
                        }
                        safe_free(editor->command_buffer);
                        editor->command_buffer = NULL;
                        editor_set_mode(editor, MODE_NORMAL);
                        break;
                        
                    case CMD_SET:
                        /* :set <option> - 設定選項 */
                        screen_show_status("設定功能待實作", false);
                        safe_free(editor->command_buffer);
                        editor->command_buffer = NULL;
                        editor_set_mode(editor, MODE_NORMAL);
                        break;
                        
                    case CMD_UNKNOWN:
                        screen_show_status("未知命令", true);
                        break;
                }
                command_free(cmd);
            } else {
                editor_set_mode(editor, MODE_NORMAL);
                safe_free(editor->command_buffer);
                editor->command_buffer = NULL;
            }
        } else {
            /* 空命令，退出命令模式 */
            editor_set_mode(editor, MODE_NORMAL);
            safe_free(editor->command_buffer);
            editor->command_buffer = NULL;
        }
    } else if (key->key == '\x1b') {
        /* Escape: 取消命令 */
        editor_set_mode(editor, MODE_NORMAL);
        safe_free(editor->command_buffer);
        editor->command_buffer = NULL;
    } else if (key->key == '\b' || key->key == '\x7f') {
        /* Backspace: 刪除命令緩衝區的最後一個字元 */
        if (editor->command_buffer != NULL) {
            size_t len = strlen(editor->command_buffer);
            if (len > 0) {
                editor->command_buffer[len - 1] = '\0';
            }
        }
    } else if (key->key >= 32 && key->key < 127) {
        /* 可列印字元: 加入命令緩衝區 */
        if (editor->command_buffer == NULL) {
            editor->command_buffer = safe_strdup("");
        }
        size_t len = strlen(editor->command_buffer);
        char *new_buf = (char *)safe_realloc(editor->command_buffer, len + 2);
        if (new_buf != NULL) {
            editor->command_buffer = new_buf;
            editor->command_buffer[len] = key->key;
            editor->command_buffer[len + 1] = '\0';
        }
    }
}

void editor_handle_input(editor_t *editor, key_input_t *key) {
    /* 參數驗證 */
    if (editor == NULL || key == NULL || editor->buffer_count == 0) {
        return;
    }
    
    buffer_t *buf = editor->buffers[editor->current_buffer];
    if (buf == NULL) {
        return;
    }
    
    /* 根據目前模式分派處理 */
    switch (editor->mode) {
        case MODE_NORMAL:
            handle_normal_mode(editor, buf, key);
            break;
            
        case MODE_INSERT:
            handle_insert_mode(editor, buf, key);
            break;
            
        case MODE_VISUAL:
            /* 可視模式（簡化實作） */
            if (key->key == '\x1b') {
                editor_set_mode(editor, MODE_NORMAL);
            }
            break;
            
        case MODE_COMMAND:
            handle_command_mode(editor, key);
            break;
    }
    
    /* 更新畫面捲動位置，確保游標在可見範圍內 */
    screen_size_t size = screen_get_size();
    size_t display_rows = size.rows - 2;  /* 保留狀態列和命令列 */
    
    if (display_rows < 1) {
        display_rows = 1;
    }
    
    if (editor->buffer_count > 0) {
        buffer_t *buf = editor->buffers[editor->current_buffer];
        if (buf != NULL) {
            if (buf->line_count < display_rows) {
                /* 檔案行數少於顯示區域，從第一行開始 */
                editor->first_line = 0;
            } else {
                /* 確保游標在可見範圍內 */
                if (editor->cursor_row < editor->first_line) {
                    editor->first_line = editor->cursor_row;
                } else if (editor->cursor_row >= editor->first_line + display_rows) {
                    editor->first_line = editor->cursor_row - display_rows + 1;
                }
                
                /* 防止 first_line 超出範圍 */
                if (editor->first_line >= buf->line_count) {
                    if (buf->line_count > display_rows) {
                        editor->first_line = buf->line_count - display_rows;
                    } else {
                        editor->first_line = 0;
                    }
                }
            }
        }
    }
}

/* ============================================================================
 * 主迴圈實作
 * ============================================================================ */

void editor_run(editor_t *editor) {
    if (editor == NULL) {
        return;
    }
    
    /* 初始化終端機輸入和畫面 */
    if (!input_init() || !screen_init()) {
        return;
    }
    
    screen_hide_cursor();
    
    bool needs_refresh = true;
    
    /* 主迴圈 */
    while (editor->running) {
        /* 只在需要時重繪畫面（減少閃爍） */
        if (needs_refresh && editor->buffer_count > 0) {
            buffer_t *buf = editor->buffers[editor->current_buffer];
            cursor_t cursor = {editor->cursor_row, editor->cursor_col};
            screen_refresh(buf, &cursor, editor->first_line);
            
            /* 建立狀態列訊息 */
            char status[256];
            const char *mode_str = "NORMAL";
            switch (editor->mode) {
                case MODE_NORMAL:  mode_str = "NORMAL";  break;
                case MODE_INSERT:  mode_str = "INSERT";  break;
                case MODE_VISUAL:  mode_str = "VISUAL";  break;
                case MODE_COMMAND: mode_str = "COMMAND"; break;
            }
            
            snprintf(status, sizeof(status), " %s | %s | 行 %zu/%zu",
                    mode_str,
                    buf->filename ? buf->filename : "[No Name]",
                    editor->cursor_row + 1,
                    buf->line_count);
            
            if (buffer_is_modified(buf)) {
                strncat(status, " [+]", sizeof(status) - strlen(status) - 1);
            }
            
            screen_show_status(status, false);
            
            /* 顯示命令列 */
            if (editor->mode == MODE_COMMAND) {
                screen_show_command(editor->command_buffer);
            } else {
                screen_show_command("");
            }
            
            needs_refresh = false;
        }
        
        /* 讀取按鍵輸入 */
        key_input_t key;
        if (input_read_key(&key)) {
            /* 記錄處理前的狀態以偵測變化 */
            size_t old_row = editor->cursor_row;
            size_t old_col = editor->cursor_col;
            editor_mode_t old_mode = editor->mode;
            
            editor_handle_input(editor, &key);
            
            /* 若有任何變化則標記需要重繪 */
            if (old_row != editor->cursor_row || 
                old_col != editor->cursor_col || 
                old_mode != editor->mode ||
                editor->mode == MODE_COMMAND) {
                needs_refresh = true;
            }
        } else {
            /* 無輸入時稍作延遲，避免 CPU 使用率過高 */
            usleep(10000);  /* 10 毫秒 */
        }
    }
    
    /* 清理終端機狀態 */
    screen_cleanup();
    input_cleanup();
}

/* ============================================================================
 * 檔案儲存實作
 * ============================================================================ */

bool editor_save(editor_t *editor) {
    if (editor == NULL || editor->buffer_count == 0) {
        return false;
    }
    
    buffer_t *buf = editor->buffers[editor->current_buffer];
    if (buf == NULL) {
        return false;
    }
    
    return buffer_save_to_file(buf, NULL);
}

bool editor_save_as(editor_t *editor, const char *filename) {
    if (editor == NULL || filename == NULL || editor->buffer_count == 0) {
        return false;
    }
    
    buffer_t *buf = editor->buffers[editor->current_buffer];
    if (buf == NULL) {
        return false;
    }
    
    return buffer_save_to_file(buf, filename);
}

/* ============================================================================
 * 模式管理實作
 * ============================================================================ */

void editor_set_mode(editor_t *editor, editor_mode_t mode) {
    if (editor != NULL) {
        editor->mode = mode;
    }
}

editor_mode_t editor_get_mode(editor_t *editor) {
    return editor ? editor->mode : MODE_NORMAL;
}
