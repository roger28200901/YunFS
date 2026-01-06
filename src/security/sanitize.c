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
 * @brief 解析並規範化路徑中的 ../ 和 ./
 * @param path 原始路徑
 * @return 規範化後的路徑，需由呼叫者釋放，失敗回傳 NULL
 */
static char *resolve_dot_dot(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    size_t len = strlen(path);
    if (len == 0) {
        return safe_strdup("");
    }
    
    /* 使用陣列模擬堆疊來處理路徑組件 */
    char **components = (char **)safe_malloc(sizeof(char *) * (len + 1));
    if (components == NULL) {
        return NULL;
    }
    
    size_t component_count = 0;
    const char *start = path;
    bool is_absolute = (path[0] == '/');
    
    /* 如果是絕對路徑，跳過開頭的斜線 */
    if (is_absolute) {
        start++;
    }
    
    /* 分割路徑為組件 */
    const char *p = start;
    while (*p != '\0') {
        /* 跳過斜線 */
        while (*p == '/') p++;
        if (*p == '\0') break;
        
        const char *comp_start = p;
        while (*p != '\0' && *p != '/') p++;
        
        size_t comp_len = p - comp_start;
        if (comp_len == 0) continue;
        
        /* 處理 "." 組件（忽略） */
        if (comp_len == 1 && comp_start[0] == '.') {
            continue;
        }
        
        /* 處理 ".." 組件 */
        if (comp_len == 2 && comp_start[0] == '.' && comp_start[1] == '.') {
            if (component_count > 0) {
                /* 移除上一個組件（回到上一層） */
                safe_free(components[component_count - 1]);
                component_count--;
            } else if (!is_absolute) {
                /* 相對路徑且已經在根目錄，保留 ".." */
                components[component_count] = safe_strndup(comp_start, comp_len);
                if (components[component_count] == NULL) {
                    /* 清理已分配的組件 */
                    for (size_t i = 0; i < component_count; i++) {
                        safe_free(components[i]);
                    }
                    safe_free(components);
                    return NULL;
                }
                component_count++;
            }
            /* 絕對路徑且 component_count == 0，忽略（已經在根目錄） */
            continue;
        }
        
        /* 一般組件 */
        components[component_count] = safe_strndup(comp_start, comp_len);
        if (components[component_count] == NULL) {
            /* 清理已分配的組件 */
            for (size_t i = 0; i < component_count; i++) {
                safe_free(components[i]);
            }
            safe_free(components);
            return NULL;
        }
        component_count++;
    }
    
    /* 構建規範化後的路徑 */
    size_t result_len = is_absolute ? 1 : 0;  /* 開頭的斜線 */
    for (size_t i = 0; i < component_count; i++) {
        result_len += strlen(components[i]) + 1;  /* 組件 + 斜線 */
    }
    
    char *result = (char *)safe_malloc(result_len + 1);
    if (result == NULL) {
        for (size_t i = 0; i < component_count; i++) {
            safe_free(components[i]);
        }
        safe_free(components);
        return NULL;
    }
    
    size_t offset = 0;
    if (is_absolute) {
        result[offset++] = '/';
    }
    
    for (size_t i = 0; i < component_count; i++) {
        if (i > 0 || is_absolute) {
            result[offset++] = '/';
        }
        size_t comp_len = strlen(components[i]);
        memcpy(result + offset, components[i], comp_len);
        offset += comp_len;
        safe_free(components[i]);
    }
    
    result[offset] = '\0';
    safe_free(components);
    
    return result;
}

/**
 * @brief 檢查路徑是否包含路徑遍歷攻擊
 * 
 * 此函式會先解析路徑中的 ../ 和 ./，然後檢查最終路徑是否超出根目錄。
 * 只有在路徑會超出根目錄時才認為是攻擊。
 */
bool is_path_traversal(const char *path) {
    if (path == NULL) {
        return false;
    }
    
    /* 先解析路徑中的 ../ 和 ./ */
    char *resolved = resolve_dot_dot(path);
    if (resolved == NULL) {
        /* 解析失敗，可能是記憶體問題，保守起見認為是攻擊 */
        error_set(ERR_PATH_TRAVERSAL, "路徑解析失敗: %s", path);
        return true;
    }
    
    /* 檢查解析後的路徑是否超出根目錄 */
    /* 如果路徑以 ../ 開頭（相對路徑），或包含 ../，則認為是攻擊 */
    if (strncmp(resolved, "../", 3) == 0 || strstr(resolved, "../") != NULL) {
        error_set(ERR_PATH_TRAVERSAL, "路徑超出根目錄: %s", path);
        safe_free(resolved);
        return true;
    }
    
    /* 檢查是否以 .. 結尾（且不是根目錄） */
    size_t len = strlen(resolved);
    if (len >= 2 && resolved[len - 2] == '.' && resolved[len - 1] == '.' && 
        (len == 2 || resolved[len - 3] == '/')) {
        error_set(ERR_PATH_TRAVERSAL, "路徑超出根目錄: %s", path);
        safe_free(resolved);
        return true;
    }
    
    /* 絕對路徑必須以 / 開頭 */
    if (path[0] == '/' && resolved[0] != '/') {
        error_set(ERR_PATH_TRAVERSAL, "路徑解析後超出根目錄: %s", path);
        safe_free(resolved);
        return true;
    }
    
    safe_free(resolved);
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
