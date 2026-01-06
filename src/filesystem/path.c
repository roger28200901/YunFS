/**
 * @file path.c
 * @brief 路徑處理模組實作
 *
 * 實作路徑字串的解析與處理功能。
 *
 * @author Yun
 * @date 2025
 */

#include "path.h"
#include "../security/validation.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/**
 * @brief 取得路徑的目錄部分
 */
char *path_get_dirname(const char *path) {
    if (path == NULL) {
        error_set(ERR_INVALID_INPUT, "路徑為 NULL");
        return NULL;
    }
    
    size_t len = strlen(path);
    if (len == 0) {
        return safe_strdup(".");
    }
    
    /* 尋找最後一個斜線位置 */
    const char *last_slash = strrchr(path, '/');
    
    if (last_slash == NULL) {
        /* 無斜線，回傳當前目錄 */
        return safe_strdup(".");
    }
    
    if (last_slash == path) {
        /* 斜線在開頭，回傳根目錄 */
        return safe_strdup("/");
    }
    
    /* 複製目錄部分 */
    size_t dir_len = last_slash - path;
    char *dir = (char *)safe_malloc(dir_len + 1);
    if (dir == NULL) {
        return NULL;
    }
    
    strncpy(dir, path, dir_len);
    dir[dir_len] = '\0';
    return dir;
}

/**
 * @brief 取得路徑的檔案名稱部分
 */
char *path_get_basename(const char *path) {
    if (path == NULL) {
        error_set(ERR_INVALID_INPUT, "路徑為 NULL");
        return NULL;
    }
    
    size_t len = strlen(path);
    if (len == 0) {
        return safe_strdup(".");
    }
    
    /* 尋找最後一個斜線位置 */
    const char *last_slash = strrchr(path, '/');
    
    if (last_slash == NULL) {
        /* 無斜線，整個路徑即為檔案名稱 */
        return safe_strdup(path);
    }
    
    /* 跳過斜線取得檔案名稱 */
    const char *basename = last_slash + 1;
    if (*basename == '\0') {
        /* 路徑以斜線結尾 */
        return safe_strdup("/");
    }
    
    return safe_strdup(basename);
}

/**
 * @brief 檢查路徑是否為絕對路徑
 */
bool path_is_absolute(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }
    return path[0] == '/';
}

/**
 * @brief 檢查路徑是否存在（預留介面）
 */
bool path_exists(const char *path) {
    /* 此函式將在 VFS 實作後補充，目前回傳 false */
    (void)path;
    return false;
}

/**
 * @brief 分割路徑為目錄與檔案名稱
 */
void path_split(const char *path, char **dir, char **file) {
    if (path == NULL) {
        if (dir) *dir = NULL;
        if (file) *file = NULL;
        return;
    }
    
    if (dir) {
        *dir = path_get_dirname(path);
    }
    
    if (file) {
        *file = path_get_basename(path);
    }
}

/**
 * @brief 取得路徑的副檔名
 */
const char *path_get_extension(const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    /* 尋找最後一個點號 */
    const char *dot = strrchr(path, '.');
    if (dot == NULL || dot == path) {
        return NULL;
    }
    
    /* 確認點號在最後一個斜線之後（避免目錄名稱中的點號） */
    const char *last_slash = strrchr(path, '/');
    if (last_slash != NULL && dot < last_slash) {
        return NULL;
    }
    
    return dot + 1;
}
