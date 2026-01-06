/**
 * @file memory.h
 * @brief 安全記憶體管理模組標頭檔
 *
 * 本模組提供安全的記憶體配置與釋放函式，包含：
 * - 自動錯誤檢查與回報
 * - 配置時自動初始化為零
 * - 敏感資料安全清除
 * - Debug 模式下的記憶體追蹤
 *
 * @author Yun
 * @date 2025
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

/* ============================================================================
 * 安全記憶體配置函式
 * ============================================================================ */

/**
 * @brief 安全的記憶體配置
 *
 * 配置指定大小的記憶體，並自動初始化為零。
 *
 * @param size 要配置的位元組數
 * @return 配置的記憶體指標，失敗回傳 NULL
 *
 * @note 配置 0 位元組會回傳 NULL 並設定錯誤
 */
void *safe_malloc(size_t size);

/**
 * @brief 安全的陣列記憶體配置
 *
 * 配置 nmemb * size 位元組的記憶體，並初始化為零。
 * 會檢查乘法溢位。
 *
 * @param nmemb 元素數量
 * @param size  單一元素大小
 * @return 配置的記憶體指標，失敗回傳 NULL
 */
void *safe_calloc(size_t nmemb, size_t size);

/**
 * @brief 安全的記憶體重新配置
 *
 * 調整已配置記憶體的大小。
 *
 * @param ptr  原有記憶體指標（可為 NULL）
 * @param size 新的大小
 * @return 新的記憶體指標，失敗回傳 NULL（原記憶體不受影響）
 */
void *safe_realloc(void *ptr, size_t size);

/**
 * @brief 安全的記憶體釋放
 *
 * 釋放記憶體，自動處理 NULL 指標。
 *
 * @param ptr 要釋放的記憶體指標（可為 NULL）
 */
void safe_free(void *ptr);

/* ============================================================================
 * 安全字串操作
 * ============================================================================ */

/**
 * @brief 安全的字串複製
 *
 * 配置新記憶體並複製字串。
 *
 * @param s 來源字串
 * @return 複製的字串指標，失敗回傳 NULL
 *
 * @note 呼叫者需負責釋放回傳的記憶體
 */
char *safe_strdup(const char *s);

/**
 * @brief 安全的限定長度字串複製
 *
 * 配置新記憶體並複製最多 n 個字元。
 *
 * @param s 來源字串
 * @param n 最大複製長度
 * @return 複製的字串指標，失敗回傳 NULL
 */
char *safe_strndup(const char *s, size_t n);

/* ============================================================================
 * 安全性函式
 * ============================================================================ */

/**
 * @brief 安全的記憶體清零
 *
 * 將記憶體內容清零，用於清除敏感資料（如密碼）。
 * 使用 volatile 防止編譯器最佳化移除此操作。
 *
 * @param ptr  記憶體指標
 * @param size 要清零的位元組數
 */
void secure_zero(void *ptr, size_t size);

/* ============================================================================
 * Debug 模式記憶體追蹤
 * ============================================================================ */

#ifdef DEBUG
/**
 * @brief 追蹤記憶體配置
 *
 * @param ptr  配置的記憶體指標
 * @param size 配置的大小
 * @param file 呼叫的原始檔名
 * @param line 呼叫的行號
 */
void memory_track_alloc(void *ptr, size_t size, const char *file, int line);

/**
 * @brief 追蹤記憶體釋放
 *
 * @param ptr  釋放的記憶體指標
 * @param file 呼叫的原始檔名
 * @param line 呼叫的行號
 */
void memory_track_free(void *ptr, const char *file, int line);

/**
 * @brief 印出記憶體使用統計
 *
 * 顯示總配置量、總釋放量，以及未釋放的記憶體區塊。
 */
void memory_print_stats(void);

#define SAFE_MALLOC(size) safe_malloc(size); memory_track_alloc(__builtin_return_address(0), size, __FILE__, __LINE__)
#define SAFE_FREE(ptr) memory_track_free(ptr, __FILE__, __LINE__); safe_free(ptr)
#else
#define SAFE_MALLOC(size) safe_malloc(size)
#define SAFE_FREE(ptr) safe_free(ptr)
#endif

#endif // MEMORY_H
