/**
 * @file sanitize.c
 * @brief 路徑清理與規範化模組實作
 *
 * 實作路徑字串的安全處理功能。
 *
 * @author Yun
 * @date 2025
 */

#include "sanitize.h"
#include "validation.h"
#include "../utils/error.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <ctype.h>

/** 最大路徑長度 */
#define MAX_PATH_LEN 4096

/* ========================================================================
 * 路徑清理函式實作
 * ======================================================================== */

/**
 * @brief 清理路徑中的危險字元
 */
char *sanitize_path(const char *path) {
    if (path == NULL) {
        error_set(ERR_INVALID_INPUT, "路徑為 NULL");
        return NULL;
    }
    
    /* 驗證路徑長度 */
    if (!validate_path_length(path, MAX_PATH_LEN)) {
        return NULL;
    }
    
    size_t len = strlen(path);
    char *sanitized = (char *)safe_malloc(len + 1);
    if (sanitized == NULL) {
        return NULL;
    }
    
    /* 過濾字元，僅保留允許的字元 */
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)path[i];
        
        /* 允許的字元：字母、數字、路徑分隔符、點、連字號、底線、空格 */
        if (isalnum(c) || c == '/' || c == '.' || c == '-' || c == '_' || c == ' ') {
            sanitized[j++] = c;
        }
        /* 跳過其他字元（包括危險的控制字元） */
    }
    
    sanitized[j] = '\0';
    return sanitized;
}

/* ========================================================================
 * 安全性檢查函式實作
 * ======================================================================== */

/**
 * @brief 檢查路徑是否包含路徑遍歷攻擊
 */
bool is_path_traversal(const char *path) {
    if (path == NULL) {
        return false;
    }
    
    /* 檢查 "../" 模式 */
    if (strstr(path, "../") != NULL) {
        error_set(ERR_PATH_TRAVERSAL, "路徑包含路徑遍歷攻擊: %s", path);
        return true;
    }
    
    /* 檢查以 ".." 開頭的情況 */
    if (strncmp(path, "..", 2) == 0 && (path[2] == '/' || path[2] == '\0')) {
        error_set(ERR_PATH_TRAVERSAL, "路徑以 .. 開頭: %s", path);
        return true;
    }
    
    /* 檢查 "/../" 模式 */
    if (strstr(path, "/../") != NULL) {
        error_set(ERR_PATH_TRAVERSAL, "路徑包含 /../: %s", path);
        return true;
    }
    
    return false;
}

/**
 * @brief 規範化路徑
 */
char *normalize_path(const char *path) {
    if (path == NULL) {
        error_set(ERR_INVALID_INPUT, "路徑為 NULL");
        return NULL;
    }
    
    /* 先檢查路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return NULL;
    }
    
    /* 驗證路徑長度 */
    if (!validate_path_length(path, MAX_PATH_LEN)) {
        return NULL;
    }
    
    size_t len = strlen(path);
    char *normalized = (char *)safe_malloc(len + 1);
    if (normalized == NULL) {
        return NULL;
    }
    
    /* 移除重複斜線並處理 ./ */
    size_t j = 0;
    bool last_was_slash = false;
    
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            if (!last_was_slash) {
                normalized[j++] = '/';
                last_was_slash = true;
            }
        } else {
            normalized[j++] = path[i];
            last_was_slash = false;
        }
    }
    
    normalized[j] = '\0';
    
    /* 移除結尾的斜線（根目錄除外） */
    if (j > 1 && normalized[j - 1] == '/') {
        normalized[j - 1] = '\0';
    }
    
    return normalized;
}

/* ========================================================================
 * 路徑處理函式實作
 * ======================================================================== */

/**
 * @brief 移除路徑中的重複斜線
 */
char *remove_duplicate_slashes(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    size_t len = strlen(path);
    char *result = (char *)safe_malloc(len + 1);
    if (result == NULL) {
        return NULL;
    }
    
    size_t j = 0;
    bool last_was_slash = false;
    
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            if (!last_was_slash) {
                result[j++] = '/';
                last_was_slash = true;
            }
        } else {
            result[j++] = path[i];
            last_was_slash = false;
        }
    }
    
    result[j] = '\0';
    return result;
}

/**
 * @brief 安全地連接兩個路徑
 */
char *safe_path_join(const char *base, const char *path) {
    if (base == NULL || path == NULL) {
        error_set(ERR_INVALID_INPUT, "路徑參數為 NULL");
        return NULL;
    }
    
    /* 檢查路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return NULL;
    }
    
    size_t base_len = strlen(base);
    size_t path_len = strlen(path);
    
    /* 檢查連接後的長度 */
    if (base_len + path_len + 2 > MAX_PATH_LEN) {
        error_set(ERR_INVALID_PATH, "連接後的路徑過長");
        return NULL;
    }
    
    char *joined = (char *)safe_malloc(base_len + path_len + 2);
    if (joined == NULL) {
        return NULL;
    }
    
    /* 複製基礎路徑 */
    strncpy(joined, base, base_len);
    joined[base_len] = '\0';
    
    /* 確保基礎路徑以斜線結尾 */
    if (base_len > 0 && base[base_len - 1] != '/') {
        strncat(joined, "/", 1);
    }
    
    /* 移除相對路徑開頭的斜線 */
    const char *path_start = path;
    if (path[0] == '/') {
        path_start = path + 1;
    }
    
    strncat(joined, path_start, path_len);
    
    /* 規範化結果 */
    char *normalized = normalize_path(joined);
    safe_free(joined);
    
    return normalized;
}
