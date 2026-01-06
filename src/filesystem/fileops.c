/**
 * @file fileops.c
 * @brief 檔案操作模組實作
 *
 * 實作安全的實體檔案系統操作，所有操作皆包含安全性驗證。
 *
 * @author Yun
 * @date 2025
 */

#include "fileops.h"
#include "../security/sanitize.h"
#include "../security/validation.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

/** 檔案大小上限：100MB */
#define MAX_FILE_SIZE (1024 * 1024 * 100)

/* ========================================================================
 * 檔案開啟與讀寫函式實作
 * ======================================================================== */

/**
 * @brief 安全地開啟檔案
 */
FILE *fileops_open(const char *path, const char *mode) {
    if (path == NULL || mode == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return NULL;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return NULL;
    }
    
    /* 規範化路徑 */
    char *normalized = normalize_path(path);
    if (normalized == NULL) {
        return NULL;
    }
    
    /* 驗證路徑長度 */
    if (!validate_path_length(normalized, 4096)) {
        safe_free(normalized);
        return NULL;
    }
    
    /* 開啟檔案 */
    FILE *file = fopen(normalized, mode);
    if (file == NULL) {
        error_set(ERR_IO_ERROR, "無法打開檔案 %s: %s", normalized, strerror(errno));
        safe_free(normalized);
        return NULL;
    }
    
    safe_free(normalized);
    return file;
}

/**
 * @brief 安全地讀取檔案內容
 */
bool fileops_read(const char *path, void **data, size_t *size) {
    if (path == NULL || data == NULL || size == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return false;
    }
    
    /* 檢查檔案是否存在 */
    if (!fileops_exists(path)) {
        error_set(ERR_FILE_NOT_FOUND, "檔案不存在: %s", path);
        return false;
    }
    
    /* 取得檔案大小 */
    size_t file_size;
    if (!fileops_get_size(path, &file_size)) {
        return false;
    }
    
    /* 檢查檔案大小限制 */
    if (file_size > MAX_FILE_SIZE) {
        error_set(ERR_INVALID_INPUT, "檔案過大: %zu 位元組", file_size);
        return false;
    }
    
    /* 開啟檔案 */
    FILE *file = fileops_open(path, "rb");
    if (file == NULL) {
        return false;
    }
    
    /* 配置記憶體緩衝區（額外 1 位元組供字串結尾） */
    void *buffer = safe_malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(file);
        return false;
    }
    
    /* 讀取檔案內容 */
    size_t read_size = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (read_size != file_size) {
        safe_free(buffer);
        error_set(ERR_IO_ERROR, "讀取檔案失敗");
        return false;
    }
    
    /* 確保以 null 結尾（便於文字檔案處理） */
    ((char *)buffer)[file_size] = '\0';
    
    *data = buffer;
    *size = file_size;
    
    return true;
}

/**
 * @brief 安全地寫入檔案內容
 */
bool fileops_write(const char *path, const void *data, size_t size) {
    if (path == NULL || data == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return false;
    }
    
    /* 檢查資料大小限制 */
    if (size > MAX_FILE_SIZE) {
        error_set(ERR_INVALID_INPUT, "資料過大: %zu 位元組", size);
        return false;
    }
    
    /* 開啟檔案（覆寫模式） */
    FILE *file = fileops_open(path, "wb");
    if (file == NULL) {
        return false;
    }
    
    /* 寫入資料 */
    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    
    if (written != size) {
        error_set(ERR_IO_ERROR, "寫入檔案失敗");
        return false;
    }
    
    return true;
}

/* ========================================================================
 * 檔案管理函式實作
 * ======================================================================== */

/**
 * @brief 安全地複製檔案
 */
bool fileops_copy(const char *src, const char *dst) {
    if (src == NULL || dst == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(src) || is_path_traversal(dst)) {
        return false;
    }
    
    /* 讀取來源檔案 */
    void *data = NULL;
    size_t size = 0;
    
    if (!fileops_read(src, &data, &size)) {
        return false;
    }
    
    /* 寫入目標檔案 */
    bool result = fileops_write(dst, data, size);
    
    /* 安全清除並釋放緩衝區 */
    secure_zero(data, size);
    safe_free(data);
    
    return result;
}

/**
 * @brief 安全地移動檔案
 */
bool fileops_move(const char *src, const char *dst) {
    if (src == NULL || dst == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(src) || is_path_traversal(dst)) {
        return false;
    }
    
    /* 嘗試使用 rename（原子操作，效能較佳） */
    char *normalized_src = normalize_path(src);
    char *normalized_dst = normalize_path(dst);
    
    if (normalized_src == NULL || normalized_dst == NULL) {
        safe_free(normalized_src);
        safe_free(normalized_dst);
        return false;
    }
    
    if (rename(normalized_src, normalized_dst) == 0) {
        safe_free(normalized_src);
        safe_free(normalized_dst);
        return true;
    }
    
    /* rename 失敗（可能跨檔案系統），改用複製後刪除 */
    safe_free(normalized_src);
    safe_free(normalized_dst);
    
    if (!fileops_copy(src, dst)) {
        return false;
    }
    
    return fileops_delete(src);
}

/**
 * @brief 安全地刪除檔案
 */
bool fileops_delete(const char *path) {
    if (path == NULL) {
        error_set(ERR_INVALID_INPUT, "路徑為 NULL");
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return false;
    }
    
    /* 規範化路徑 */
    char *normalized = normalize_path(path);
    if (normalized == NULL) {
        return false;
    }
    
    /* 檢查檔案是否存在 */
    if (!fileops_exists(normalized)) {
        safe_free(normalized);
        error_set(ERR_FILE_NOT_FOUND, "檔案不存在: %s", path);
        return false;
    }
    
    /* 刪除檔案 */
    if (unlink(normalized) != 0) {
        error_set(ERR_IO_ERROR, "無法刪除檔案 %s: %s", normalized, strerror(errno));
        safe_free(normalized);
        return false;
    }
    
    safe_free(normalized);
    return true;
}

/* ========================================================================
 * 檔案資訊函式實作
 * ======================================================================== */

/**
 * @brief 檢查檔案是否存在
 */
bool fileops_exists(const char *path) {
    if (path == NULL) {
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return false;
    }
    
    struct stat st;
    char *normalized = normalize_path(path);
    if (normalized == NULL) {
        return false;
    }
    
    bool exists = (stat(normalized, &st) == 0);
    safe_free(normalized);
    
    return exists;
}

/**
 * @brief 取得檔案大小
 */
bool fileops_get_size(const char *path, size_t *size) {
    if (path == NULL || size == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return false;
    }
    
    struct stat st;
    char *normalized = normalize_path(path);
    if (normalized == NULL) {
        return false;
    }
    
    if (stat(normalized, &st) != 0) {
        safe_free(normalized);
        error_set(ERR_FILE_NOT_FOUND, "無法獲取檔案資訊: %s", path);
        return false;
    }
    
    *size = (size_t)st.st_size;
    safe_free(normalized);
    
    return true;
}

/**
 * @brief 檢查檔案權限
 */
bool fileops_check_permission(const char *path, const char *mode) {
    if (path == NULL || mode == NULL) {
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return false;
    }
    
    char *normalized = normalize_path(path);
    if (normalized == NULL) {
        return false;
    }
    
    /* 解析權限模式字串 */
    int access_mode = 0;
    if (strchr(mode, 'r') != NULL) access_mode |= R_OK;
    if (strchr(mode, 'w') != NULL) access_mode |= W_OK;
    if (strchr(mode, 'x') != NULL) access_mode |= X_OK;
    
    bool has_permission = (access(normalized, access_mode) == 0);
    safe_free(normalized);
    
    return has_permission;
}
