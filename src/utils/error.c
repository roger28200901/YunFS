/**
 * @file error.c
 * @brief 錯誤處理模組實作
 *
 * 實作統一的錯誤處理機制。
 *
 * @author Yun
 * @date 2025
 */

#include "error.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * 模組內部狀態
 * ============================================================================ */

/** 全域錯誤狀態 */
static error_t g_error = {ERR_OK, "", 0, NULL};

/* ============================================================================
 * 錯誤處理函式
 * ============================================================================ */

/**
 * @brief 設定錯誤狀態
 */
void error_set(error_code_t code, const char *format, ...) {
    g_error.code = code;
    g_error.line = __LINE__;
    g_error.file = __FILE__;
    
    va_list args;
    va_start(args, format);
    vsnprintf(g_error.message, sizeof(g_error.message), format, args);
    va_end(args);
    
    /* 確保字串以 null 結尾 */
    g_error.message[sizeof(g_error.message) - 1] = '\0';
}

/**
 * @brief 取得目前的錯誤狀態
 */
error_t error_get(void) {
    return g_error;
}

/**
 * @brief 清除錯誤狀態
 */
void error_clear(void) {
    g_error.code = ERR_OK;
    g_error.message[0] = '\0';
    g_error.line = 0;
    g_error.file = NULL;
}

/**
 * @brief 輸出錯誤訊息
 */
void error_print(FILE *stream) {
    if (g_error.code == ERR_OK) {
        return;
    }
    
    fprintf(stream, "錯誤 [%s:%d]: %s (%s)\n",
            g_error.file ? g_error.file : "unknown",
            g_error.line,
            g_error.message,
            error_code_to_string(g_error.code));
}

/**
 * @brief 將錯誤代碼轉換為字串
 */
const char *error_code_to_string(error_code_t code) {
    switch (code) {
        case ERR_OK:              return "成功";
        case ERR_MEMORY:          return "記憶體錯誤";
        case ERR_INVALID_INPUT:   return "無效輸入";
        case ERR_FILE_NOT_FOUND:  return "檔案不存在";
        case ERR_PERMISSION:      return "權限不足";
        case ERR_PATH_TRAVERSAL:  return "路徑遍歷攻擊";
        case ERR_BUFFER_OVERFLOW: return "緩衝區溢位";
        case ERR_INVALID_PATH:    return "無效路徑";
        case ERR_IO_ERROR:        return "I/O 錯誤";
        case ERR_UNKNOWN:         return "未知錯誤";
        default:                  return "未定義錯誤";
    }
}
