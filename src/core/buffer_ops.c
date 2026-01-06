/**
 * @file buffer_ops.c
 * @brief 緩衝區進階操作模組實作
 * 
 * 實作 Vim 風格的進階文字編輯操作。
 */

#include "buffer_ops.h"
#include "buffer.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * 私有輔助函式
 * ============================================================================ */

/**
 * @brief 擴展行的容量（內部使用）
 * 
 * 與 buffer.c 中的 expand_line 邏輯相同，
 * 因為該函式為 static，故在此複製實作。
 * 
 * @param line 目標行
 * @param min_capacity 最小需要的容量
 * @return 成功回傳 true，失敗回傳 false
 */
static bool expand_line_internal(line_t *line, size_t min_capacity) {
    if (line == NULL) {
        return false;
    }
    
    /* 若現有容量已足夠，無需擴展 */
    if (line->capacity >= min_capacity) {
        return true;
    }
    
    /* 採用倍增策略 */
    size_t new_capacity = line->capacity * 2;
    if (new_capacity < min_capacity) {
        new_capacity = min_capacity;
    }
    
    char *new_text = (char *)safe_realloc(line->text, new_capacity);
    if (new_text == NULL) {
        return false;
    }
    
    line->text = new_text;
    line->capacity = new_capacity;
    return true;
}

/* ============================================================================
 * 字元分類輔助函式實作
 * ============================================================================ */

bool is_word_char(char c) {
    /* 單詞字元：字母、數字、底線 */
    return isalnum(c) || c == '_';
}

bool is_whitespace(char c) {
    /* 空白字元：空格、Tab */
    return c == ' ' || c == '\t';
}

/* ============================================================================
 * 字元操作實作
 * ============================================================================ */

bool buffer_replace_char(buffer_t *buf, size_t line_num, size_t col, char c) {
    /* 參數驗證 */
    if (buf == NULL || buf->read_only) {
        return false;
    }
    
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col >= line->length) {
        return false;
    }
    
    /* 直接替換字元 */
    line->text[col] = c;
    buf->modified = true;
    return true;
}

/* ============================================================================
 * 行操作實作
 * ============================================================================ */

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

bool buffer_split_line(buffer_t *buf, size_t line_num, size_t col) {
    /* 參數驗證 */
    if (buf == NULL || buf->read_only) {
        return false;
    }
    
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col > line->length) {
        return false;
    }
    
    /* 複製分割點之後的文字 */
    char *second_part = safe_strdup(line->text + col);
    if (second_part == NULL) {
        return false;
    }
    
    /* 截斷原行 */
    line->text[col] = '\0';
    line->length = col;
    
    /* 插入新行 */
    buffer_insert_line(buf, line_num + 1, second_part);
    safe_free(second_part);
    buf->modified = true;
    return true;
}

/* ============================================================================
 * 區段刪除操作實作
 * ============================================================================ */

bool buffer_delete_to_end(buffer_t *buf, size_t line_num, size_t col) {
    /* 參數驗證 */
    if (buf == NULL || buf->read_only) {
        return false;
    }
    
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col >= line->length) {
        return false;
    }
    
    /* 直接截斷 */
    line->text[col] = '\0';
    line->length = col;
    buf->modified = true;
    return true;
}

bool buffer_delete_to_start(buffer_t *buf, size_t line_num, size_t col) {
    /* 參數驗證 */
    if (buf == NULL || buf->read_only) {
        return false;
    }
    
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col == 0 || col > line->length) {
        return false;
    }
    
    /* 將後半部移到開頭 */
    memmove(line->text, line->text + col, line->length - col + 1);
    line->length -= col;
    buf->modified = true;
    return true;
}

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

bool buffer_delete_word_backward(buffer_t *buf, size_t line_num, size_t col, size_t *new_col) {
    /* 參數驗證 */
    if (buf == NULL || buf->read_only) {
        return false;
    }
    
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col == 0) {
        return false;
    }
    
    size_t end = col;
    size_t start = col;
    
    /* 向後搜尋單詞開頭 */
    if (start > 0) {
        start--;
        if (is_word_char(line->text[start])) {
            /* 找到單詞開頭 */
            while (start > 0 && is_word_char(line->text[start - 1])) {
                start--;
            }
        } else if (is_whitespace(line->text[start])) {
            /* 找到空白開頭 */
            while (start > 0 && is_whitespace(line->text[start - 1])) {
                start--;
            }
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

/* ============================================================================
 * 文字複製操作實作
 * ============================================================================ */

char *buffer_copy_line(buffer_t *buf, size_t line_num, size_t *len) {
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL) {
        return NULL;
    }
    
    char *text = safe_strdup(line->text);
    if (len != NULL) {
        *len = line->length;
    }
    return text;
}

char *buffer_copy_to_end(buffer_t *buf, size_t line_num, size_t col, size_t *len) {
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col >= line->length) {
        return NULL;
    }
    
    size_t copy_len = line->length - col;
    char *text = safe_strndup(line->text + col, copy_len);
    if (len != NULL) {
        *len = copy_len;
    }
    return text;
}

char *buffer_copy_to_start(buffer_t *buf, size_t line_num, size_t col, size_t *len) {
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col == 0 || col > line->length) {
        return NULL;
    }
    
    char *text = safe_strndup(line->text, col);
    if (len != NULL) {
        *len = col;
    }
    return text;
}

char *buffer_copy_word(buffer_t *buf, size_t line_num, size_t col, size_t *len) {
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col >= line->length) {
        return NULL;
    }
    
    size_t start = col;
    size_t end = col;
    
    /* 根據字元類型決定單詞範圍 */
    if (is_word_char(line->text[start])) {
        while (end < line->length && is_word_char(line->text[end])) {
            end++;
        }
    } else if (is_whitespace(line->text[start])) {
        while (end < line->length && is_whitespace(line->text[end])) {
            end++;
        }
    } else {
        end++;
    }
    
    if (end > start) {
        char *text = safe_strndup(line->text + start, end - start);
        if (len != NULL) {
            *len = end - start;
        }
        return text;
    }
    
    return NULL;
}

/* ============================================================================
 * 文字插入與替換實作
 * ============================================================================ */

bool buffer_insert_text(buffer_t *buf, size_t line_num, size_t col, const char *text) {
    /* 參數驗證 */
    if (buf == NULL || buf->read_only || text == NULL) {
        return false;
    }
    
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL) {
        return false;
    }
    
    size_t text_len = strlen(text);
    
    /* 擴展容量 */
    if (!expand_line_internal(line, line->length + text_len + 1)) {
        return false;
    }
    
    /* 移動後續文字並插入新文字 */
    memmove(line->text + col + text_len, line->text + col, line->length - col + 1);
    memcpy(line->text + col, text, text_len);
    line->length += text_len;
    buf->modified = true;
    return true;
}

bool buffer_replace_text(buffer_t *buf, size_t line_num, size_t col_start, size_t col_end, const char *text) {
    /* 參數驗證 */
    if (buf == NULL || buf->read_only) {
        return false;
    }
    
    line_t *line = buffer_get_line(buf, line_num);
    if (line == NULL || col_start > col_end || col_end > line->length) {
        return false;
    }
    
    size_t old_len = col_end - col_start;
    size_t new_len = text ? strlen(text) : 0;
    size_t diff = new_len - old_len;
    
    /* 若長度有變化，需要調整記憶體 */
    if (diff != 0) {
        if (!expand_line_internal(line, line->length + diff + 1)) {
            return false;
        }
        memmove(line->text + col_end + diff, line->text + col_end, line->length - col_end + 1);
    }
    
    /* 複製新文字 */
    if (text != NULL && new_len > 0) {
        memcpy(line->text + col_start, text, new_len);
    }
    
    line->length += diff;
    buf->modified = true;
    return true;
}

/* ============================================================================
 * 文字取得（別名函式）
 * ============================================================================ */

char *buffer_get_to_end(buffer_t *buf, size_t line_num, size_t col, size_t *len) {
    return buffer_copy_to_end(buf, line_num, col, len);
}

char *buffer_get_to_start(buffer_t *buf, size_t line_num, size_t col, size_t *len) {
    return buffer_copy_to_start(buf, line_num, col, len);
}
