# Core 模組重點程式碼說明

本文件說明 `src/core/` 目錄下各模組的核心實作與設計重點。

---

## 目錄結構

```
src/core/
├── buffer.c/h          # 文字緩衝區核心模組
├── buffer_ops.c/h      # 緩衝區進階操作
├── editor.c/h          # 編輯器核心模組
├── vim_ops.c/h         # Vim 操作模組
├── shell.c/h           # Shell 核心模組
├── shell_commands.c/h  # Shell 命令處理
├── shell_completion.c/h # Shell Tab 自動完成
└── command.c/h         # 命令解析模組
```

---

## 1. Buffer 模組 (buffer.c/h)

### 1.1 核心資料結構

```30:36:src/core/buffer.h
typedef struct line {
    char *text;              /**< 行內容（以 null 結尾的字串） */
    size_t length;           /**< 行長度（不包括結尾的 null 字元） */
    size_t capacity;         /**< 已配置的記憶體容量 */
    struct line *next;       /**< 指向下一行的指標 */
    struct line *prev;       /**< 指向上一行的指標 */
} line_t;
```

**設計重點：**
- 使用雙向鏈結串列儲存文字行，支援高效的插入與刪除
- 每行獨立管理記憶體容量，採用動態擴展策略
- 避免大區塊重新配置，提升效能

```44:51:src/core/buffer.h
typedef struct {
    char *filename;          /**< 關聯的檔案名稱（可為 NULL） */
    line_t *head;            /**< 指向第一行的指標 */
    line_t *tail;            /**< 指向最後一行的指標 */
    size_t line_count;       /**< 總行數 */
    bool modified;           /**< 是否有未儲存的修改 */
    bool read_only;          /**< 是否為唯讀模式 */
} buffer_t;
```

### 1.2 記憶體擴展策略

```157:183:src/core/buffer.c
static bool expand_line(line_t *line, size_t min_capacity) {
    if (line == NULL) {
        return false;
    }
    
    /* 若現有容量已足夠，無需擴展 */
    if (line->capacity >= min_capacity) {
        return true;
    }
    
    /* 採用倍增策略計算新容量 */
    size_t new_capacity = line->capacity * 2;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }
    
    /* 重新配置記憶體 */
    char *new_text = (char *)safe_realloc(line->text, new_capacity);
    if (new_text == NULL) {
        return false;
    }
    
    line->text = new_text;
    line->capacity = new_capacity;
    
    return true;
}
```

**重點說明：**
- 採用倍增策略（capacity * 2），減少重新配置次數
- 若倍增後仍不足，則直接使用所需的最小容量
- 使用 `safe_realloc` 確保記憶體安全

### 1.3 行插入操作

```319:385:src/core/buffer.c
bool buffer_insert_line(buffer_t *buf, size_t line_num, const char *text) {
    /* 參數驗證 */
    if (buf == NULL) {
        error_set(ERR_INVALID_INPUT, "緩衝區為 NULL");
        return false;
    }
    
    /* 檢查唯讀狀態 */
    if (buf->read_only) {
        error_set(ERR_PERMISSION, "緩衝區為唯讀");
        return false;
    }
    
    /* 建立新行 */
    line_t *new_line = create_line(text);
    if (new_line == NULL) {
        return false;
    }
    
    /* 根據插入位置處理鏈結串列 */
    if (line_num == 0 || buf->head == NULL) {
        /* 情況一：插入到最前面 */
        new_line->next = buf->head;
        if (buf->head != NULL) {
            buf->head->prev = new_line;
        }
        buf->head = new_line;
        if (buf->tail == NULL) {
            buf->tail = new_line;
        }
    } else if (line_num >= buf->line_count) {
        /* 情況二：插入到最後面 */
        new_line->prev = buf->tail;
        if (buf->tail != NULL) {
            buf->tail->next = new_line;
        }
        buf->tail = new_line;
        if (buf->head == NULL) {
            buf->head = new_line;
        }
    } else {
        /* 情況三：插入到中間位置 */
        line_t *curr = buffer_get_line(buf, line_num);
        if (curr == NULL) {
            destroy_line(new_line);
            return false;
        }
        
        /* 調整鏈結關係 */
        new_line->prev = curr->prev;
        new_line->next = curr;
        if (curr->prev != NULL) {
            curr->prev->next = new_line;
        }
        curr->prev = new_line;
        
        /* 若插入位置是第一行，更新 head */
        if (curr == buf->head) {
            buf->head = new_line;
        }
    }
    
    buf->line_count++;
    buf->modified = true;
    
    return true;
}
```

**重點說明：**
- 處理三種插入情況：最前面、最後面、中間位置
- 正確維護雙向鏈結串列的前後指標
- 自動更新 `head`、`tail` 和 `line_count`

---

## 2. Buffer Operations 模組 (buffer_ops.c/h)

### 2.1 單詞刪除操作

```190:233:src/core/buffer_ops.c
bool buffer_delete_word(buffer_t *buf, size_t line_num, size_t col, size_t *new_col) {
    /* 參數驗證 */
    if (buf == NULL || buf->read_only) {
        return false;
    }
    
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col >= line->length) {
        return false;
    }
    
    size_t start = col;
    size_t end = col;
    
    /* 根據目前字元類型決定刪除範圍 */
    if (start < line->length) {
        if (is_word_char(line->text[start])) {
            /* 刪除整個單詞 */
            while (end < line->length && is_word_char(line->text[end])) {
                end++;
            }
        } else if (is_whitespace(line->text[start])) {
            /* 刪除連續空白 */
            while (end < line->length && is_whitespace(line->text[end])) {
                end++;
            }
        } else {
            /* 刪除單一特殊字元 */
            end++;
        }
    }
    
    /* 執行刪除 */
    if (end > start) {
        memmove(line->text + start, line->text + end, line->length - end + 1);
        line->length -= (end - start);
        buf->modified = true;
    }
    
    if (new_col != NULL) {
        *new_col = start;
    }
    return true;
}
```

**重點說明：**
- 實作 Vim 的 `dw` 命令行為
- 根據字元類型（單詞字元、空白、特殊字元）決定刪除範圍
- 使用 `memmove` 安全地移動記憶體

### 2.2 行合併操作

```94:120:src/core/buffer_ops.c
bool buffer_join_lines(buffer_t *buf, size_t line_num) {
    /* 參數驗證：確保不是最後一行 */
    if (buf == NULL || buf->read_only || line_num >= buf->line_count - 1) {
        return false;
    }
    
    line_t *line1 = buffer_get_line(buf, line_num);
    line_t *line2 = buffer_get_line(buf, line_num + 1);
    if (line1 == NULL || line2 == NULL) {
        return false;
    }
    
    /* 計算合併後的長度並擴展容量 */
    size_t new_len = line1->length + line2->length;
    if (!expand_line_internal(line1, new_len + 1)) {
        return false;
    }
    
    /* 將第二行內容附加到第一行 */
    memcpy(line1->text + line1->length, line2->text, line2->length + 1);
    line1->length = new_len;
    
    /* 刪除第二行 */
    buffer_delete_line(buf, line_num + 1);
    buf->modified = true;
    return true;
}
```

**重點說明：**
- 實作 Vim 的 `J` 命令（合併兩行）
- 先擴展第一行容量，再複製第二行內容
- 最後刪除第二行，完成合併

---

## 3. Editor 模組 (editor.c/h)

### 3.1 編輯器主結構

```73:86:src/core/editor.h
typedef struct editor {
    buffer_t **buffers;        /**< 緩衝區指標陣列 */
    size_t buffer_count;       /**< 目前開啟的緩衝區數量 */
    size_t current_buffer;     /**< 目前作用中的緩衝區索引 */
    size_t cursor_row;         /**< 游標行位置 */
    size_t cursor_col;         /**< 游標列位置 */
    editor_mode_t mode;        /**< 目前的編輯模式 */
    size_t first_line;         /**< 畫面顯示的起始行號（用於捲動） */
    char *command_buffer;      /**< 命令模式的輸入緩衝區 */
    bool running;              /**< 編輯器是否正在執行 */
    vim_context_t *vim_ctx;    /**< Vim 操作上下文 */
    size_t repeat_count;       /**< 重複次數（如 3dd 中的 3） */
    char last_operation;       /**< 最後執行的操作（用於 . 命令） */
} editor_t;
```

**設計重點：**
- 支援多緩衝區編輯（最多 16 個）
- 四種編輯模式：Normal、Insert、Visual、Command
- 整合 Vim 上下文，支援進階操作

### 3.2 Normal 模式處理

```211:285:src/core/editor.c
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
```

**重點說明：**
- 實作 Vim 風格的按鍵映射（h/j/k/l 移動）
- 游標移動時自動調整欄位，避免超出行長度
- 支援模式切換（i/a/v/:）

### 3.3 主迴圈與畫面更新

```558:638:src/core/editor.c
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
```

**重點說明：**
- 採用增量更新策略（`needs_refresh`），只在狀態變化時重繪
- 比較處理前後的狀態，判斷是否需要重繪
- 無輸入時使用 `usleep` 降低 CPU 使用率

---

## 4. Vim Operations 模組 (vim_ops.c/h)

### 4.1 撤銷記錄系統

```79:87:src/core/vim_ops.h
typedef struct undo_record {
    undo_type_t type;           /**< 操作類型 */
    size_t row;                 /**< 操作發生的行號 */
    size_t col;                 /**< 操作發生的欄位 */
    char *text;                 /**< 相關的文字內容 */
    size_t text_len;            /**< 文字長度 */
    struct undo_record *prev;   /**< 前一筆記錄 */
    struct undo_record *next;   /**< 後一筆記錄 */
} undo_record_t;
```

```81:133:src/core/vim_ops.c
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
```

**重點說明：**
- 使用雙向鏈結串列儲存撤銷記錄
- 新記錄插入到串列頭，保持時間順序
- 超過上限時自動移除最舊的記錄，避免記憶體無限增長

### 4.2 搜尋功能

```329:376:src/core/vim_ops.c
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
```

**重點說明：**
- 實作循環搜尋：從目前位置搜尋到檔案尾，再從頭繼續
- 使用 `strstr` 進行字串匹配
- 找到匹配後自動移動游標到該位置

---

## 5. Shell 模組 (shell.c/h)

### 5.1 Shell 主結構

```23:30:src/core/shell.h
typedef struct {
    vfs_t *vfs;                     /**< 虛擬文件系統實例 */
    vfs_node_t *current_dir;        /**< 當前工作目錄節點 */
    char *prompt;                   /**< 命令提示符字串 */
    bool running;                   /**< Shell 運行狀態旗標 */
    char *history[HISTORY_MAX];     /**< 命令歷史記錄陣列 */
    int history_count;              /**< 當前歷史記錄數量 */
} shell_t;
```

### 5.2 命令表驅動設計

```60:77:src/core/shell.c
static const cmd_entry_t cmd_table[] = {
    { "ls",      cmd_ls      },
    { "cd",      cmd_cd      },
    { "pwd",     cmd_pwd     },
    { "mkdir",   cmd_mkdir   },
    { "touch",   cmd_touch   },
    { "cat",     cmd_cat     },
    { "echo",    cmd_echo    },
    { "rm",      cmd_rm      },
    { "mv",      cmd_mv      },
    { "cp",      cmd_cp      },
    { "vim",     cmd_vim     },
    { "clear",   cmd_clear   },
    { "help",    cmd_help    },
    { "history", cmd_history },
    { "exit",    cmd_exit    },
    { NULL,      NULL        }  // 結束標記
};
```

```207:236:src/core/shell.c
bool shell_execute_command(shell_t *shell, const char *command) {
    if (shell == NULL || command == NULL) {
        return false;
    }
    
    // 跳過前導空白
    while (*command && isspace(*command)) command++;
    if (!*command) return true;  // 空命令視為成功
    
    // 解析命令
    int argc = 0;
    char **argv = shell_parse_command(command, &argc);
    if (argv == NULL || argc == 0) {
        return false;
    }
    
    // 查找並執行命令
    bool result = false;
    cmd_handler_t handler = find_command_handler(argv[0]);
    
    if (handler != NULL) {
        result = handler(shell, argc, argv);
    } else {
        printf("錯誤: 未知命令 '%s'。輸入 'help' 查看可用命令\n", argv[0]);
        result = false;
    }
    
    shell_free_args(argv, argc);
    return result;
}
```

**重點說明：**
- 使用表驅動方式管理命令，便於擴展新命令
- 統一的命令處理函數簽名：`bool cmd_xxx(shell_t *shell, int argc, char **argv)`
- 自動解析命令參數並分派到對應的處理函數

---

## 6. Shell Commands 模組 (shell_commands.c/h)

### 6.1 路徑解析輔助函數

```80:129:src/core/shell_commands.c
char *shell_get_full_path(shell_t *shell, const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    // 處理特殊路徑 "."（當前目錄）
    if (strcmp(path, ".") == 0) {
        return vfs_get_path(shell->current_dir);
    }
    
    // 處理特殊路徑 ".."（父目錄）
    if (strcmp(path, "..") == 0) {
        if (shell->current_dir->parent != NULL) {
            return vfs_get_path(shell->current_dir->parent);
        } else {
            return safe_strdup("/");
        }
    }
    
    // 絕對路徑直接返回副本
    if (path[0] == '/') {
        return safe_strdup(path);
    }
    
    // 相對路徑：與當前目錄路徑連接
    char *current_path = vfs_get_path(shell->current_dir);
    if (current_path == NULL) {
        return NULL;
    }
    
    size_t current_len = strlen(current_path);
    size_t path_len = strlen(path);
    char *full_path = (char *)safe_malloc(current_len + path_len + 2);
    if (full_path == NULL) {
        safe_free(current_path);
        return NULL;
    }
    
    strncpy(full_path, current_path, current_len);
    // 確保路徑分隔符
    if (current_path[current_len - 1] != '/') {
        full_path[current_len] = '/';
        current_len++;
    }
    strncpy(full_path + current_len, path, path_len);
    full_path[current_len + path_len] = '\0';
    
    safe_free(current_path);
    return full_path;
}
```

**重點說明：**
- 處理特殊路徑 "." 和 ".."
- 區分絕對路徑和相對路徑
- 自動處理路徑分隔符，確保路徑格式正確

### 6.2 Vim 編輯器整合

```558:664:src/core/shell_commands.c
bool cmd_vim(shell_t *shell, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: vim <檔案名稱>\n");
        return false;
    }
    
    char *full_path = shell_get_full_path(shell, argv[1]);
    if (full_path == NULL) {
        printf("錯誤: 無法解析路徑\n");
        return false;
    }
    
    // 創建臨時檔案名稱（將路徑中的 / 替換為 _）
    char *tmp_filename = (char *)safe_malloc(strlen(full_path) + 10);
    if (tmp_filename == NULL) {
        safe_free(full_path);
        return false;
    }
    strcpy(tmp_filename, ".tmp_");
    for (size_t i = 0; i < strlen(full_path); i++) {
        if (full_path[i] == '/') {
            tmp_filename[strlen(tmp_filename)] = '_';
        } else {
            tmp_filename[strlen(tmp_filename)] = full_path[i];
        }
    }
    tmp_filename[strlen(tmp_filename)] = '\0';
    
    // 從 VFS 讀取檔案內容到臨時檔案
    vfs_node_t *file = vfs_find_node(shell->vfs, full_path);
    if (file != NULL && file->type == VFS_FILE) {
        size_t size = 0;
        void *data = vfs_read_file(file, &size);
        FILE *tmp_file = fopen(tmp_filename, "w");
        if (tmp_file != NULL) {
            if (data != NULL && size > 0) {
                fwrite(data, 1, size, tmp_file);
            }
            fclose(tmp_file);
            if (data != NULL) {
                safe_free(data);
            }
        } else if (data != NULL) {
            safe_free(data);
        }
    } else {
        // 檔案不存在，創建空的臨時檔案
        FILE *tmp_file = fopen(tmp_filename, "w");
        if (tmp_file != NULL) {
            fclose(tmp_file);
        }
    }
    
    // 創建並運行編輯器
    editor_t *editor = editor_create();
    if (editor == NULL) {
        printf("錯誤: 無法創建編輯器\n");
        safe_free(tmp_filename);
        safe_free(full_path);
        return false;
    }
    
    if (editor_open_file(editor, tmp_filename)) {
        editor_run(editor);
        
        // 編輯完成後，將臨時檔案內容保存回 VFS
        FILE *saved_file = fopen(tmp_filename, "r");
        if (saved_file != NULL) {
            fseek(saved_file, 0, SEEK_END);
            long size = ftell(saved_file);
            fseek(saved_file, 0, SEEK_SET);
            
            if (size > 0) {
                void *data = safe_malloc(size);
                if (data != NULL) {
                    fread(data, 1, size, saved_file);
                    
                    if (file == NULL) {
                        vfs_create_file(shell->vfs, full_path, data, size);
                    } else {
                        vfs_write_file(file, data, size);
                    }
                    
                    safe_free(data);
                }
            } else {
                // 空檔案處理
                if (file == NULL) {
                    vfs_create_file(shell->vfs, full_path, NULL, 0);
                } else {
                    vfs_write_file(file, NULL, 0);
                }
            }
            
            fclose(saved_file);
        }
        
        // 清理臨時檔案
        remove(tmp_filename);
    }
    
    editor_destroy(editor);
    safe_free(tmp_filename);
    safe_free(full_path);
    
    return true;
}
```

**重點說明：**
- 使用臨時檔案作為 VFS 與編輯器之間的橋樑
- 編輯前從 VFS 讀取內容寫入臨時檔案
- 編輯後從臨時檔案讀取內容寫回 VFS
- 自動清理臨時檔案

---

## 7. Shell Completion 模組 (shell_completion.c/h)

### 7.1 Tab 自動完成

```146:228:src/core/shell_completion.c
static void handle_tab_completion(shell_t *shell, char *buffer, 
                                   size_t *pos, size_t *cursor, size_t size) {
    // 找到當前正在輸入的詞的起始位置
    size_t word_start = *cursor;
    while (word_start > 0 && buffer[word_start - 1] != ' ') {
        word_start--;
    }
    
    // 提取當前詞
    char word[256];
    size_t word_len = *cursor - word_start;
    if (word_len >= sizeof(word)) return;
    
    strncpy(word, buffer + word_start, word_len);
    word[word_len] = '\0';
    
    // 獲取補全選項
    size_t comp_count = 0;
    char **completions = shell_get_completions(shell, word, &comp_count);
    
    if (comp_count == 1) {
        // 唯一匹配：直接補全
        size_t comp_len = strlen(completions[0]);
        size_t add_len = comp_len - word_len;
        
        if (add_len > 0 && *pos + add_len < size - 1) {
            // 移動游標後的內容，插入補全部分
            memmove(buffer + *cursor + add_len, buffer + *cursor, *pos - *cursor + 1);
            memcpy(buffer + *cursor, completions[0] + word_len, add_len);
            *pos += add_len;
            
            // 更新顯示
            printf("%s", buffer + *cursor);
            *cursor += add_len;
            if (*pos > *cursor) {
                printf("\033[%zuD", *pos - *cursor);
            }
        }
    } else if (comp_count > 1) {
        // 多個匹配：嘗試補全共同前綴
        char *common = shell_find_common_prefix(completions, comp_count);
        if (common != NULL) {
            size_t common_len = strlen(common);
            if (common_len > word_len) {
                // 有共同前綴可以補全
                size_t add_len = common_len - word_len;
                if (*pos + add_len < size - 1) {
                    memmove(buffer + *cursor + add_len, buffer + *cursor, *pos - *cursor + 1);
                    memcpy(buffer + *cursor, common + word_len, add_len);
                    *pos += add_len;
                    
                    printf("%s", buffer + *cursor);
                    *cursor += add_len;
                    if (*pos > *cursor) {
                        printf("\033[%zuD", *pos - *cursor);
                    }
                }
            } else {
                // 無法進一步補全，顯示所有選項
                printf("\n");
                for (size_t i = 0; i < comp_count; i++) {
                    printf("%s  ", completions[i]);
                }
                printf("\n");
                
                // 重新顯示提示符和當前輸入
                char *current_path = vfs_get_path(shell->current_dir);
                if (current_path != NULL) {
                    printf("\033[32m%s\033[0m ", current_path);
                    safe_free(current_path);
                }
                printf("%s%s", shell->prompt, buffer);
                if (*pos > *cursor) {
                    printf("\033[%zuD", *pos - *cursor);
                }
            }
            safe_free(common);
        }
    }
    
    shell_free_completions(completions, comp_count);
    fflush(stdout);
}
```

**重點說明：**
- 自動識別當前正在輸入的詞（以空格分隔）
- 唯一匹配時直接補全
- 多個匹配時補全共同前綴，或顯示所有選項

---

## 8. Command 模組 (command.c/h)

### 8.1 命令解析

```19:143:src/core/command.c
command_t *command_parse(const char *cmd_str) {
    /* 參數驗證 */
    if (cmd_str == NULL || strlen(cmd_str) == 0) {
        return NULL;
    }
    
    /* 配置命令結構 */
    command_t *cmd = (command_t *)safe_malloc(sizeof(command_t));
    if (cmd == NULL) {
        return NULL;
    }
    
    /* 初始化預設值 */
    cmd->type = CMD_UNKNOWN;
    cmd->arg1 = NULL;
    cmd->arg2 = NULL;
    cmd->force = false;
    
    /* 跳過前導空白字元 */
    const char *p = cmd_str;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    /* 提取命令名稱（直到空白、斜線或字串結尾） */
    char cmd_name[32];
    size_t i = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '/' && i < sizeof(cmd_name) - 1) {
        cmd_name[i++] = *p++;
    }
    cmd_name[i] = '\0';
    
    /* 根據命令名稱判斷類型並解析參數 */
    
    if (strcmp(cmd_name, "q") == 0) {
        /* :q 或 :q! - 退出命令 */
        cmd->type = CMD_QUIT;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '!') {
            cmd->type = CMD_QUIT_FORCE;
            cmd->force = true;
        }
    } 
    else if (strcmp(cmd_name, "w") == 0) {
        /* :w 或 :wq - 儲存命令 */
        cmd->type = CMD_WRITE;
        while (*p == ' ' || *p == '\t') p++;
        
        if (*p == 'q') {
            /* :wq - 儲存並退出 */
            cmd->type = CMD_WRITE_QUIT;
        } else if (*p != '\0') {
            /* :w <filename> - 另存新檔 */
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '\0') {
                size_t len = strlen(p);
                cmd->arg1 = safe_strndup(p, len);
            }
        }
    } 
    else if (strcmp(cmd_name, "wq") == 0) {
        /* :wq - 儲存並退出 */
        cmd->type = CMD_WRITE_QUIT;
    } 
    else if (strcmp(cmd_name, "e") == 0 || strcmp(cmd_name, "edit") == 0) {
        /* :e <file> 或 :edit <file> - 開啟檔案 */
        cmd->type = CMD_EDIT;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '\0') {
            size_t len = strlen(p);
            cmd->arg1 = safe_strndup(p, len);
        }
    } 
    else if (strcmp(cmd_name, "b") == 0 || strcmp(cmd_name, "buffer") == 0) {
        /* :b <n> 或 :buffer <n> - 切換緩衝區 */
        cmd->type = CMD_BUFFER;
        while (*p == ' ' || *p != '\t') p++;
        if (*p != '\0') {
            size_t len = strlen(p);
            cmd->arg1 = safe_strndup(p, len);
        }
    } 
    else if (strcmp(cmd_name, "s") == 0 || strcmp(cmd_name, "substitute") == 0) {
        /* :s/old/new/ - 搜尋替換 */
        cmd->type = CMD_SUBSTITUTE;
        
        /* 解析 /old/new/ 格式 */
        if (*p == '/') {
            p++;
            const char *old_start = p;
            
            /* 找到第一個分隔符 */
            while (*p != '\0' && *p != '/') p++;
            
            if (*p == '/') {
                size_t old_len = p - old_start;
                cmd->arg1 = safe_strndup(old_start, old_len);
                p++;
                
                const char *new_start = p;
                /* 找到第二個分隔符 */
                while (*p != '\0' && *p != '/') p++;
                size_t new_len = p - new_start;
                cmd->arg2 = safe_strndup(new_start, new_len);
            }
        }
    } 
    else if (strcmp(cmd_name, "set") == 0) {
        /* :set <option> - 設定選項 */
        cmd->type = CMD_SET;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '\0') {
            size_t len = strlen(p);
            cmd->arg1 = safe_strndup(p, len);
        }
    } 
    else if (cmd_name[0] == '/') {
        /* /pattern - 搜尋命令（以斜線開頭） */
        cmd->type = CMD_SEARCH;
        cmd->arg1 = safe_strdup(cmd_name + 1);  /* 跳過開頭的斜線 */
    }
    /* 其他情況保持 CMD_UNKNOWN */
    
    return cmd;
}
```

**重點說明：**
- 支援多種 Vim 風格命令格式（:q, :w, :wq, :e, :s/old/new/ 等）
- 解析命令參數和修飾符（如 `!` 表示強制執行）
- 使用狀態機方式解析複雜格式（如搜尋替換的 `/old/new/`）

---

## 總結

### 核心設計原則

1. **模組化設計**：各模組職責清晰，低耦合高內聚
2. **記憶體安全**：使用 `safe_malloc`、`safe_free` 等安全函數
3. **錯誤處理**：統一的錯誤處理機制（`error_set`、`error_get`）
4. **效能優化**：增量更新、記憶體擴展策略、延遲重繪
5. **擴展性**：表驅動設計、統一的函數簽名

### 關鍵技術點

- **雙向鏈結串列**：用於緩衝區行管理和撤銷記錄
- **表驅動設計**：用於 Shell 命令分派
- **狀態機**：用於命令解析和模式切換
- **臨時檔案橋接**：用於 VFS 與編輯器的整合

