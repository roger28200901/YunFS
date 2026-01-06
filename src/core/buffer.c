/**
 * @file buffer.c
 * @brief 文字緩衝區模組實作
 * 
 * 實作編輯器核心的文字緩衝區功能，包含行的建立、刪除、
 * 字元插入/刪除，以及檔案讀寫等操作。
 */

#define _POSIX_C_SOURCE 200809L  /* 啟用 POSIX 擴充功能（如 getline） */

#include "buffer.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include "../security/validation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ============================================================================
 * 私有常數定義
 * ============================================================================ */

/** @brief 新建行的初始容量（位元組） */
#define INITIAL_LINE_CAPACITY 256

/* ============================================================================
 * 私有函式宣告
 * ============================================================================ */

/**
 * @brief 建立新的文字行
 * @param text 行的初始文字內容（可為 NULL，視為空字串）
 * @return 新建立的行指標，失敗時回傳 NULL
 */
static line_t *create_line(const char *text);

/**
 * @brief 銷毀文字行並釋放記憶體
 * @param line 要銷毀的行（可為 NULL）
 * @note 會安全清除行內容以防止資料洩漏
 */
static void destroy_line(line_t *line);

/**
 * @brief 擴展行的容量
 * @param line 目標行
 * @param min_capacity 最小需要的容量
 * @return 成功回傳 true，失敗回傳 false
 * @note 採用倍增策略以減少重新配置次數
 */
static bool expand_line(line_t *line, size_t min_capacity);

/* ============================================================================
 * 緩衝區生命週期管理實作
 * ============================================================================ */

buffer_t *buffer_create(const char *filename) {
    /* 配置緩衝區結構 */
    buffer_t *buf = (buffer_t *)safe_malloc(sizeof(buffer_t));
    if (buf == NULL) {
        return NULL;
    }
    
    /* 初始化所有欄位 */
    buf->filename = filename ? safe_strdup(filename) : NULL;
    buf->head = NULL;
    buf->tail = NULL;
    buf->line_count = 0;
    buf->modified = false;
    buf->read_only = false;
    
    /* 建立初始空行（緩衝區至少需要一行） */
    line_t *empty_line = create_line("");
    if (empty_line == NULL) {
        safe_free(buf->filename);
        safe_free(buf);
        return NULL;
    }
    
    buf->head = empty_line;
    buf->tail = empty_line;
    buf->line_count = 1;
    
    return buf;
}

void buffer_destroy(buffer_t *buf) {
    if (buf == NULL) {
        return;
    }
    
    /* 遍歷並銷毀所有行 */
    line_t *line = buf->head;
    while (line != NULL) {
        line_t *next = line->next;
        destroy_line(line);
        line = next;
    }
    
    safe_free(buf->filename);
    safe_free(buf);
}

/* ============================================================================
 * 私有行操作實作
 * ============================================================================ */

static line_t *create_line(const char *text) {
    /* 處理 NULL 輸入 */
    if (text == NULL) {
        text = "";
    }
    
    /* 計算所需容量（至少為初始容量） */
    size_t len = strlen(text);
    size_t capacity = (len < INITIAL_LINE_CAPACITY) ? INITIAL_LINE_CAPACITY : len + 1;
    
    /* 配置行結構 */
    line_t *line = (line_t *)safe_malloc(sizeof(line_t));
    if (line == NULL) {
        return NULL;
    }
    
    /* 配置文字儲存空間 */
    line->text = (char *)safe_malloc(capacity);
    if (line->text == NULL) {
        safe_free(line);
        return NULL;
    }
    
    /* 複製文字內容並初始化欄位 */
    strncpy(line->text, text, len);
    line->text[len] = '\0';
    line->length = len;
    line->capacity = capacity;
    line->next = NULL;
    line->prev = NULL;
    
    return line;
}

static void destroy_line(line_t *line) {
    if (line == NULL) {
        return;
    }
    
    /* 安全清除文字內容（防止敏感資料殘留於記憶體） */
    if (line->text != NULL) {
        secure_zero(line->text, line->length);
        safe_free(line->text);
    }
    
    safe_free(line);
}

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

/* ============================================================================
 * 檔案輸入/輸出實作
 * ============================================================================ */

bool buffer_load_from_file(buffer_t *buf, const char *filename) {
    /* 參數驗證 */
    if (buf == NULL || filename == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return false;
    }
    
    /* 開啟檔案 */
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        error_set(ERR_FILE_NOT_FOUND, "無法開啟檔案: %s", filename);
        return false;
    }
    
    /* 清空現有內容 */
    line_t *line = buf->head;
    while (line != NULL) {
        line_t *next = line->next;
        destroy_line(line);
        line = next;
    }
    
    buf->head = NULL;
    buf->tail = NULL;
    buf->line_count = 0;
    
    /* 逐行讀取檔案 */
    char *line_buffer = NULL;
    size_t buffer_size = 0;
    ssize_t read_len;
    
    while ((read_len = getline(&line_buffer, &buffer_size, file)) != -1) {
        /* 移除行尾的換行符號 */
        if (read_len > 0 && line_buffer[read_len - 1] == '\n') {
            line_buffer[read_len - 1] = '\0';
            read_len--;
        }
        
        /* 建立新行並加入鏈結串列 */
        line_t *new_line = create_line(line_buffer);
        if (new_line == NULL) {
            free(line_buffer);
            fclose(file);
            return false;
        }
        
        /* 將新行接到串列尾端 */
        if (buf->head == NULL) {
            buf->head = new_line;
            buf->tail = new_line;
        } else {
            new_line->prev = buf->tail;
            buf->tail->next = new_line;
            buf->tail = new_line;
        }
        
        buf->line_count++;
    }
    
    free(line_buffer);
    fclose(file);
    
    /* 確保至少有一行（空檔案的情況） */
    if (buf->head == NULL) {
        line_t *empty_line = create_line("");
        if (empty_line == NULL) {
            return false;
        }
        buf->head = empty_line;
        buf->tail = empty_line;
        buf->line_count = 1;
    }
    
    /* 更新檔案名稱並清除修改標記 */
    safe_free(buf->filename);
    buf->filename = safe_strdup(filename);
    buf->modified = false;
    
    return true;
}

bool buffer_save_to_file(buffer_t *buf, const char *filename) {
    /* 參數驗證 */
    if (buf == NULL) {
        error_set(ERR_INVALID_INPUT, "緩衝區為 NULL");
        return false;
    }
    
    /* 決定儲存的檔案名稱 */
    const char *save_filename = filename ? filename : buf->filename;
    if (save_filename == NULL) {
        error_set(ERR_INVALID_INPUT, "檔案名稱為 NULL");
        return false;
    }
    
    /* 開啟檔案進行寫入 */
    FILE *file = fopen(save_filename, "w");
    if (file == NULL) {
        error_set(ERR_IO_ERROR, "無法寫入檔案: %s", save_filename);
        return false;
    }
    
    /* 逐行寫入檔案 */
    line_t *line = buf->head;
    while (line != NULL) {
        if (fprintf(file, "%s\n", line->text) < 0) {
            fclose(file);
            error_set(ERR_IO_ERROR, "寫入檔案失敗");
            return false;
        }
        line = line->next;
    }
    
    fclose(file);
    
    /* 若指定了新檔名，更新緩衝區的檔案名稱 */
    if (filename != NULL) {
        safe_free(buf->filename);
        buf->filename = safe_strdup(filename);
    }
    
    /* 清除修改標記 */
    buf->modified = false;
    return true;
}

/* ============================================================================
 * 行操作實作
 * ============================================================================ */

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

bool buffer_delete_line(buffer_t *buf, size_t line_num) {
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
    
    /* 特殊情況：只剩一行時，清空內容而非刪除 */
    if (buf->line_count == 1) {
        line_t *line = buf->head;
        secure_zero(line->text, line->length);
        line->text[0] = '\0';
        line->length = 0;
        buf->modified = true;
        return true;
    }
    
    /* 取得要刪除的行 */
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL) {
        return false;
    }
    
    /* 調整鏈結關係 */
    if (line->prev != NULL) {
        line->prev->next = line->next;
    } else {
        buf->head = line->next;
    }
    
    if (line->next != NULL) {
        line->next->prev = line->prev;
    } else {
        buf->tail = line->prev;
    }
    
    /* 銷毀該行並更新計數 */
    destroy_line(line);
    buf->line_count--;
    buf->modified = true;
    
    return true;
}

line_t *buffer_get_line(buffer_t *buf, size_t line_num) {
    /* 參數驗證 */
    if (buf == NULL || buf->head == NULL) {
        return NULL;
    }
    
    /* 若行號超出範圍，回傳最後一行 */
    if (line_num >= buf->line_count) {
        return buf->tail;
    }
    
    /* 從頭遍歷到指定行 */
    line_t *line = buf->head;
    for (size_t i = 0; i < line_num && line != NULL; i++) {
        line = line->next;
    }
    
    return line;
}

/* ============================================================================
 * 字元操作實作
 * ============================================================================ */

bool buffer_insert_char(buffer_t *buf, size_t line_num, size_t col, char c) {
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
    
    /* 取得目標行 */
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL) {
        return false;
    }
    
    /* 修正欄位索引（若超出行長度則插入到行尾） */
    if (col > line->length) {
        col = line->length;
    }
    
    /* 確保有足夠容量（需要多一個字元空間） */
    if (!expand_line(line, line->length + 2)) {
        return false;
    }
    
    /* 移動後續字元並插入新字元 */
    memmove(line->text + col + 1, line->text + col, line->length - col + 1);
    line->text[col] = c;
    line->length++;
    buf->modified = true;
    
    return true;
}

bool buffer_delete_char(buffer_t *buf, size_t line_num, size_t col) {
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
    
    /* 取得目標行 */
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || line->length == 0) {
        return false;
    }
    
    /* 修正欄位索引 */
    if (col >= line->length) {
        col = line->length - 1;
    }
    
    /* 移動後續字元覆蓋被刪除的字元 */
    memmove(line->text + col, line->text + col + 1, line->length - col);
    line->length--;
    line->text[line->length] = '\0';
    buf->modified = true;
    
    return true;
}

/* ============================================================================
 * 狀態查詢與設定實作
 * ============================================================================ */

size_t buffer_get_line_count(buffer_t *buf) {
    return buf ? buf->line_count : 0;
}

void buffer_mark_modified(buffer_t *buf) {
    if (buf != NULL) {
        buf->modified = true;
    }
}

void buffer_clear_modified(buffer_t *buf) {
    if (buf != NULL) {
        buf->modified = false;
    }
}

bool buffer_is_modified(buffer_t *buf) {
    return buf ? buf->modified : false;
}
