/**
 * @file vim_ops.c
 * @brief Vim 操作模組實作
 * 
 * 實作 Vim 編輯器的進階功能，包含撤銷/重做、暫存器、搜尋等。
 */

#include "vim_ops.h"
#include "editor.h"
#include "buffer_ops.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* ============================================================================
 * 私有常數定義
 * ============================================================================ */

/** @brief 最大撤銷記錄數量 */
#define MAX_UNDO_RECORDS 1000

/* ============================================================================
 * 上下文管理實作
 * ============================================================================ */

void vim_init_context(vim_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    /* 清零所有欄位 */
    memset(ctx, 0, sizeof(vim_context_t));
    
    /* 設定初始值 */
    ctx->op_type = VIM_OP_NONE;
    ctx->undo_max = MAX_UNDO_RECORDS;
    ctx->search_direction = 1;  /* 預設向前搜尋 */
    
    /* 初始化所有暫存器 */
    for (int i = 0; i < 26; i++) {
        ctx->registers[i].text = NULL;
        ctx->registers[i].length = 0;
        ctx->registers[i].is_line = false;
    }
    
    ctx->default_register.text = NULL;
    ctx->default_register.length = 0;
    ctx->default_register.is_line = false;
}

void vim_cleanup_context(vim_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    /* 釋放所有撤銷記錄 */
    undo_record_t *record = ctx->undo_head;
    while (record != NULL) {
        undo_record_t *next = record->next;
        safe_free(record->text);
        safe_free(record);
        record = next;
    }
    
    /* 釋放所有暫存器內容 */
    for (int i = 0; i < 26; i++) {
        safe_free(ctx->registers[i].text);
    }
    safe_free(ctx->default_register.text);
    
    /* 釋放搜尋模式 */
    safe_free(ctx->search_pattern);
}

/* ============================================================================
 * 撤銷/重做實作
 * ============================================================================ */

void vim_record_undo(vim_context_t *ctx, undo_type_t type, size_t row, size_t col, const char *text, size_t len) {
    if (ctx == NULL) {
        return;
    }
    
    /* 配置新的撤銷記錄 */
    undo_record_t *record = (undo_record_t *)safe_malloc(sizeof(undo_record_t));
    if (record == NULL) {
        return;
    }
    
    /* 填入記錄資訊 */
    record->type = type;
    record->row = row;
    record->col = col;
    record->text = NULL;
    record->text_len = 0;
    
    /* 複製相關文字 */
    if (text != NULL && len > 0) {
        record->text = safe_strndup(text, len);
        if (record->text != NULL) {
            record->text_len = len;
        }
    }
    
    /* 插入到串列頭（最新的記錄在前） */
    record->prev = NULL;
    record->next = ctx->undo_head;
    
    if (ctx->undo_head != NULL) {
        ctx->undo_head->prev = record;
    }
    ctx->undo_head = record;
    
    if (ctx->undo_tail == NULL) {
        ctx->undo_tail = record;
    }
    
    ctx->undo_count++;
    
    /* 若超過上限，移除最舊的記錄 */
    while (ctx->undo_count > ctx->undo_max && ctx->undo_tail != NULL) {
        undo_record_t *old = ctx->undo_tail;
        ctx->undo_tail = old->prev;
        if (ctx->undo_tail != NULL) {
            ctx->undo_tail->next = NULL;
        }
        safe_free(old->text);
        safe_free(old);
        ctx->undo_count--;
    }
}

/* ============================================================================
 * 暫存器操作實作
 * ============================================================================ */

void vim_yank_to_register(vim_context_t *ctx, char reg, const char *text, size_t len, bool is_line) {
    if (ctx == NULL || text == NULL) {
        return;
    }
    
    /* 決定目標暫存器 */
    vim_register_t *target = &ctx->default_register;
    if (reg >= 'a' && reg <= 'z') {
        target = &ctx->registers[reg - 'a'];
    } else if (reg >= 'A' && reg <= 'Z') {
        /* 大寫字母表示追加模式 */
        target = &ctx->registers[reg - 'A'];
    }
    
    if (reg >= 'A' && reg <= 'Z' && target->text != NULL) {
        /* 追加模式：將新文字附加到現有內容後 */
        size_t old_len = target->length;
        char *new_text = (char *)safe_realloc(target->text, old_len + len + 1);
        if (new_text != NULL) {
            target->text = new_text;
            memcpy(target->text + old_len, text, len);
            target->text[old_len + len] = '\0';
            target->length = old_len + len;
        }
    } else {
        /* 替換模式：清除舊內容，儲存新內容 */
        safe_free(target->text);
        target->text = safe_strndup(text, len);
        target->length = len;
        target->is_line = is_line;
    }
}

const char *vim_get_register(vim_context_t *ctx, char reg, size_t *len, bool *is_line) {
    if (ctx == NULL) {
        return NULL;
    }
    
    /* 決定來源暫存器 */
    vim_register_t *target = &ctx->default_register;
    if (reg >= 'a' && reg <= 'z') {
        target = &ctx->registers[reg - 'a'];
    }
    
    /* 回傳資訊 */
    if (len != NULL) {
        *len = target->length;
    }
    if (is_line != NULL) {
        *is_line = target->is_line;
    }
    
    return target->text;
}

/* ============================================================================
 * 文字物件操作實作
 * ============================================================================ */

bool vim_find_word_start(buffer_t *buf, size_t row, size_t col, size_t *new_col) {
    line_t *line = buffer_get_line(buf, row);
    if (line == NULL || col >= line->length) {
        return false;
    }
    
    size_t pos = col;
    
    if (pos < line->length && is_word_char(line->text[pos])) {
        /* 若在單詞中，向前找到單詞開頭 */
        while (pos > 0 && is_word_char(line->text[pos - 1])) {
            pos--;
        }
    } else {
        /* 跳過非單詞字元和空白 */
        while (pos < line->length && !is_word_char(line->text[pos]) && !is_whitespace(line->text[pos])) {
            pos++;
        }
        while (pos < line->length && is_whitespace(line->text[pos])) {
            pos++;
        }
    }
    
    if (new_col != NULL) {
        *new_col = pos;
    }
    return true;
}

bool vim_find_word_end(buffer_t *buf, size_t row, size_t col, size_t *new_col) {
    line_t *line = buffer_get_line(buf, row);
    if (line == NULL || col >= line->length) {
        return false;
    }
    
    size_t pos = col;
    
    /* 根據目前字元類型決定移動方式 */
    if (pos < line->length && is_word_char(line->text[pos])) {
        /* 跳過整個單詞 */
        while (pos < line->length && is_word_char(line->text[pos])) {
            pos++;
        }
    } else if (pos < line->length && is_whitespace(line->text[pos])) {
        /* 跳過空白 */
        while (pos < line->length && is_whitespace(line->text[pos])) {
            pos++;
        }
    } else {
        pos++;
    }
    
    if (new_col != NULL) {
        *new_col = pos;
    }
    return true;
}

bool vim_find_word_backward(buffer_t *buf, size_t row, size_t col, size_t *new_row, size_t *new_col) {
    size_t curr_row = row;
    size_t curr_col = col;
    
    if (curr_col > 0) {
        line_t *line = buffer_get_line(buf, curr_row);
        if (line != NULL) {
            curr_col--;
            /* 根據字元類型向後搜尋 */
            if (curr_col < line->length && is_word_char(line->text[curr_col])) {
                while (curr_col > 0 && is_word_char(line->text[curr_col - 1])) {
                    curr_col--;
                }
            } else {
                while (curr_col > 0 && is_whitespace(line->text[curr_col - 1])) {
                    curr_col--;
                }
            }
        }
    } else if (curr_row > 0) {
        /* 移到上一行尾端 */
        curr_row--;
        line_t *line = buffer_get_line(buf, curr_row);
        if (line != NULL) {
            curr_col = line->length;
        }
    } else {
        return false;
    }
    
    if (new_row != NULL) {
        *new_row = curr_row;
    }
    if (new_col != NULL) {
        *new_col = curr_col;
    }
    return true;
}

bool vim_find_line_start(buffer_t *buf, size_t row, size_t *new_col) {
    line_t *line = buffer_get_line(buf, row);
    if (line == NULL) {
        return false;
    }
    
    /* 跳過前導空白（^ 命令的行為） */
    size_t pos = 0;
    while (pos < line->length && is_whitespace(line->text[pos])) {
        pos++;
    }
    
    if (new_col != NULL) {
        *new_col = pos;
    }
    return true;
}

bool vim_find_line_end(buffer_t *buf, size_t row, size_t *new_col) {
    line_t *line = buffer_get_line(buf, row);
    if (line == NULL) {
        return false;
    }
    
    if (new_col != NULL) {
        *new_col = line->length;
    }
    return true;
}

/* ============================================================================
 * 搜尋操作實作
 * ============================================================================ */

bool vim_search_forward(editor_t *editor, const char *pattern) {
    /* 參數驗證 */
    if (editor == NULL || pattern == NULL || editor->buffer_count == 0) {
        return false;
    }
    
    buffer_t *buf = editor->buffers[editor->current_buffer];
    if (buf == NULL) {
        return false;
    }
    
    size_t start_row = editor->cursor_row;
    size_t start_col = editor->cursor_col + 1;  /* 從目前位置的下一個字元開始 */
    
    /* 從目前位置向後搜尋 */
    for (size_t row = start_row; row < buf->line_count; row++) {
        line_t *line = buffer_get_line(buf, row);
        if (line == NULL) {
            continue;
        }
        
        size_t start = (row == start_row) ? start_col : 0;
        const char *found = strstr(line->text + start, pattern);
        if (found != NULL) {
            editor->cursor_row = row;
            editor->cursor_col = found - line->text;
            return true;
        }
    }
    
    /* 從檔案開頭繼續搜尋（循環搜尋） */
    for (size_t row = 0; row <= start_row; row++) {
        line_t *line = buffer_get_line(buf, row);
        if (line == NULL) {
            continue;
        }
        
        size_t end = (row == start_row) ? start_col : line->length;
        const char *found = strstr(line->text, pattern);
        if (found != NULL && (size_t)(found - line->text) < end) {
            editor->cursor_row = row;
            editor->cursor_col = found - line->text;
            return true;
        }
    }
    
    return false;
}

bool vim_search_backward(editor_t *editor, const char *pattern) {
    /* 參數驗證 */
    if (editor == NULL || pattern == NULL || editor->buffer_count == 0) {
        return false;
    }
    
    buffer_t *buf = editor->buffers[editor->current_buffer];
    if (buf == NULL) {
        return false;
    }
    
    size_t start_row = editor->cursor_row;
    size_t start_col = editor->cursor_col;
    
    /* 從目前位置向前搜尋 */
    for (size_t row = start_row; row < buf->line_count; row--) {
        line_t *line = buffer_get_line(buf, row);
        if (line == NULL) {
            if (row == 0) break;
            continue;
        }
        
        size_t end = (row == start_row) ? start_col : line->length;
        const char *last_found = NULL;
        const char *found = line->text;
        
        /* 找到該範圍內的最後一個匹配 */
        while ((found = strstr(found, pattern)) != NULL && (size_t)(found - line->text) < end) {
            last_found = found;
            found++;
        }
        
        if (last_found != NULL) {
            editor->cursor_row = row;
            editor->cursor_col = last_found - line->text;
            return true;
        }
        
        if (row == 0) break;
    }
    
    /* 從檔案尾端繼續搜尋（循環搜尋） */
    for (size_t row = buf->line_count - 1; row > start_row; row--) {
        line_t *line = buffer_get_line(buf, row);
        if (line == NULL) {
            continue;
        }
        
        const char *last_found = NULL;
        const char *found = line->text;
        
        while ((found = strstr(found, pattern)) != NULL) {
            last_found = found;
            found++;
        }
        
        if (last_found != NULL) {
            editor->cursor_row = row;
            editor->cursor_col = last_found - line->text;
            return true;
        }
    }
    
    return false;
}

bool vim_search_next(editor_t *editor, vim_context_t *ctx) {
    if (editor == NULL || ctx == NULL || ctx->search_pattern == NULL) {
        return false;
    }
    
    /* 依原方向搜尋 */
    if (ctx->search_direction > 0) {
        return vim_search_forward(editor, ctx->search_pattern);
    } else {
        return vim_search_backward(editor, ctx->search_pattern);
    }
}

bool vim_search_prev(editor_t *editor, vim_context_t *ctx) {
    if (editor == NULL || ctx == NULL || ctx->search_pattern == NULL) {
        return false;
    }
    
    /* 依反方向搜尋 */
    if (ctx->search_direction > 0) {
        return vim_search_backward(editor, ctx->search_pattern);
    } else {
        return vim_search_forward(editor, ctx->search_pattern);
    }
}
