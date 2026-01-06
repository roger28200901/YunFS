/**
 * @file validation.c
 * @brief 輸入驗證模組實作
 *
 * 實作各種輸入驗證功能，確保程式安全性。
 *
 * @author Yun
 * @date 2025
 */

#include "validation.h"
#include "../utils/error.h"
#include <string.h>
#include <ctype.h>
#include <limits.h>

/** 最大路徑長度（POSIX 標準） */
#define MAX_PATH_LEN 4096

/** 最大檔案名稱長度 */
#define MAX_FILENAME_LEN 255

/* ========================================================================
 * 字串驗證函式實作
 * ======================================================================== */

/**
 * @brief 驗證字串長度是否在安全範圍內
 */
bool validate_string_length(const char *str, size_t max_len) {
    if (str == NULL) {
        error_set(ERR_INVALID_INPUT, "字串為 NULL");
        return false;
    }
    
    /* 使用 strnlen 避免在超長字串上花費過多時間 */
    size_t len = strnlen(str, max_len + 1);
    if (len > max_len) {
        error_set(ERR_BUFFER_OVERFLOW, "字串長度超過限制: %zu > %zu", len, max_len);
        return false;
    }
    
    return true;
}

/**
 * @brief 驗證字串是否僅包含有效字元
 */
bool validate_string_chars(const char *str, const char *allowed_chars) {
    if (str == NULL) {
        error_set(ERR_INVALID_INPUT, "字串為 NULL");
        return false;
    }
    
    if (allowed_chars == NULL) {
        /* 預設允許所有可列印字元與換行、Tab */
        for (size_t i = 0; str[i] != '\0'; i++) {
            if (!isprint((unsigned char)str[i]) && str[i] != '\n' && str[i] != '\t') {
                error_set(ERR_INVALID_INPUT, "字串包含無效字元");
                return false;
            }
        }
        return true;
    }
    
    /* 檢查每個字元是否在允許列表中 */
    for (size_t i = 0; str[i] != '\0'; i++) {
        bool found = false;
        for (size_t j = 0; allowed_chars[j] != '\0'; j++) {
            if (str[i] == allowed_chars[j]) {
                found = true;
                break;
            }
        }
        if (!found) {
            error_set(ERR_INVALID_INPUT, "字串包含不允許的字元: '%c'", str[i]);
            return false;
        }
    }
    
    return true;
}

/* ========================================================================
 * 緩衝區驗證函式實作
 * ======================================================================== */

/**
 * @brief 驗證緩衝區存取邊界
 */
bool validate_buffer_bounds(size_t offset, size_t size, size_t buffer_size) {
    /* 檢查偏移量是否超出緩衝區 */
    if (offset > buffer_size) {
        error_set(ERR_BUFFER_OVERFLOW, "偏移量超出緩衝區大小");
        return false;
    }
    
    /* 檢查存取範圍是否超出緩衝區（避免整數溢位） */
    if (size > buffer_size - offset) {
        error_set(ERR_BUFFER_OVERFLOW, "大小超出緩衝區邊界");
        return false;
    }
    
    return true;
}

/* ========================================================================
 * 數值驗證函式實作
 * ======================================================================== */

/**
 * @brief 驗證整數是否在指定範圍內
 */
bool validate_int_range(int value, int min, int max) {
    if (value < min || value > max) {
        error_set(ERR_INVALID_INPUT, "整數超出範圍: %d (範圍: %d-%d)", value, min, max);
        return false;
    }
    return true;
}

/* ========================================================================
 * 檔案系統驗證函式實作
 * ======================================================================== */

/**
 * @brief 驗證檔案名稱是否有效
 */
bool validate_filename(const char *filename) {
    if (filename == NULL) {
        error_set(ERR_INVALID_INPUT, "檔案名稱為 NULL");
        return false;
    }
    
    /* 檢查長度限制 */
    if (!validate_string_length(filename, MAX_FILENAME_LEN)) {
        return false;
    }
    
    /* 檢查是否為空字串 */
    if (filename[0] == '\0') {
        error_set(ERR_INVALID_INPUT, "檔案名稱為空");
        return false;
    }
    
    /* 檢查禁止的字元（根據 POSIX 規範） */
    const char *forbidden = "/\0";
    for (size_t i = 0; filename[i] != '\0'; i++) {
        for (size_t j = 0; forbidden[j] != '\0'; j++) {
            if (filename[i] == forbidden[j]) {
                error_set(ERR_INVALID_INPUT, "檔案名稱包含禁止字元: '%c'", filename[i]);
                return false;
            }
        }
    }
    
    /* 禁止以 ".." 開頭（安全考量） */
    if (filename[0] == '.' && filename[1] == '.') {
        error_set(ERR_INVALID_INPUT, "檔案名稱不能以 '..' 開頭");
        return false;
    }
    
    return true;
}

/**
 * @brief 驗證路徑長度
 */
bool validate_path_length(const char *path, size_t max_len) {
    if (path == NULL) {
        error_set(ERR_INVALID_INPUT, "路徑為 NULL");
        return false;
    }
    
    /* 若未指定最大長度，使用預設值 */
    size_t limit = (max_len > 0) ? max_len : MAX_PATH_LEN;
    return validate_string_length(path, limit);
}
