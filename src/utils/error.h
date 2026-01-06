/**
 * @file error.h
 * @brief 錯誤處理模組標頭檔
 *
 * 本模組提供統一的錯誤處理機制，包含：
 * - 錯誤代碼定義
 * - 錯誤訊息設定與取得
 * - 錯誤輸出與清除
 *
 * @author Yun
 * @date 2025
 */

#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

/* ============================================================================
 * 錯誤代碼定義
 * ============================================================================ */

/**
 * @brief 錯誤代碼列舉
 */
typedef enum {
    ERR_OK = 0,           /**< 無錯誤 */
    ERR_MEMORY,           /**< 記憶體配置失敗 */
    ERR_INVALID_INPUT,    /**< 無效輸入 */
    ERR_FILE_NOT_FOUND,   /**< 檔案不存在 */
    ERR_PERMISSION,       /**< 權限不足 */
    ERR_PATH_TRAVERSAL,   /**< 路徑遍歷攻擊 */
    ERR_BUFFER_OVERFLOW,  /**< 緩衝區溢位 */
    ERR_INVALID_PATH,     /**< 無效路徑 */
    ERR_IO_ERROR,         /**< I/O 錯誤 */
    ERR_UNKNOWN           /**< 未知錯誤 */
} error_code_t;

/* ============================================================================
 * 錯誤結構定義
 * ============================================================================ */

/**
 * @brief 錯誤資訊結構
 */
typedef struct {
    error_code_t code;    /**< 錯誤代碼 */
    char message[256];    /**< 錯誤訊息 */
    int line;             /**< 發生錯誤的行號 */
    const char *file;     /**< 發生錯誤的檔案 */
} error_t;

/* ============================================================================
 * 錯誤處理函式
 * ============================================================================ */

/**
 * @brief 設定錯誤狀態
 *
 * 記錄錯誤代碼與格式化訊息。
 *
 * @param code   錯誤代碼
 * @param format 格式化字串（類似 printf）
 * @param ...    格式化參數
 */
void error_set(error_code_t code, const char *format, ...);

/**
 * @brief 取得目前的錯誤狀態
 *
 * @return 目前的錯誤資訊結構
 */
error_t error_get(void);

/**
 * @brief 清除錯誤狀態
 *
 * 將錯誤代碼重設為 ERR_OK。
 */
void error_clear(void);

/**
 * @brief 輸出錯誤訊息
 *
 * 將錯誤訊息輸出到指定的串流。
 *
 * @param stream 輸出串流（如 stderr）
 */
void error_print(FILE *stream);

/**
 * @brief 將錯誤代碼轉換為字串
 *
 * @param code 錯誤代碼
 * @return 對應的錯誤描述字串
 */
const char *error_code_to_string(error_code_t code);

/* ============================================================================
 * 便利巨集
 * ============================================================================ */

/**
 * @brief 檢查錯誤並處理
 *
 * 若表達式結果不為 ERR_OK，印出錯誤並回傳 -1。
 */
#define CHECK_ERROR(expr) \
    do { \
        if ((expr) != ERR_OK) { \
            error_print(stderr); \
            return -1; \
        } \
    } while(0)

/**
 * @brief 致命錯誤處理
 *
 * 印出錯誤訊息並終止程式。
 */
#define FATAL_ERROR(msg) \
    do { \
        fprintf(stderr, "致命錯誤 [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
        exit(EXIT_FAILURE); \
    } while(0)

#endif // ERROR_H
