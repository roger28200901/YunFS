/**
 * @file validation.h
 * @brief 輸入驗證模組標頭檔
 *
 * 本模組提供各種輸入驗證功能，用於防止：
 * - 緩衝區溢位攻擊
 * - 注入攻擊
 * - 無效輸入導致的程式錯誤
 *
 * @author Yun
 * @date 2025
 */

#ifndef VALIDATION_H
#define VALIDATION_H

#include <stddef.h>
#include <stdbool.h>

/* ========================================================================
 * 字串驗證函式
 * ======================================================================== */

/**
 * @brief 驗證字串長度是否在安全範圍內
 *
 * @param str     要驗證的字串
 * @param max_len 最大允許長度
 * @return true 長度有效，false 超過限制或為 NULL
 */
bool validate_string_length(const char *str, size_t max_len);

/**
 * @brief 驗證字串是否僅包含有效字元
 *
 * 用於防止注入攻擊，確保字串只包含允許的字元。
 *
 * @param str           要驗證的字串
 * @param allowed_chars 允許的字元集（NULL 表示允許所有可列印字元）
 * @return true 所有字元皆有效，false 包含無效字元
 */
bool validate_string_chars(const char *str, const char *allowed_chars);

/* ========================================================================
 * 緩衝區驗證函式
 * ======================================================================== */

/**
 * @brief 驗證緩衝區存取邊界
 *
 * 檢查指定的偏移量與大小是否在緩衝區範圍內。
 *
 * @param offset      存取起始偏移量
 * @param size        要存取的大小
 * @param buffer_size 緩衝區總大小
 * @return true 存取範圍有效，false 超出邊界
 */
bool validate_buffer_bounds(size_t offset, size_t size, size_t buffer_size);

/* ========================================================================
 * 數值驗證函式
 * ======================================================================== */

/**
 * @brief 驗證整數是否在指定範圍內
 *
 * @param value 要驗證的整數
 * @param min   最小值（含）
 * @param max   最大值（含）
 * @return true 在範圍內，false 超出範圍
 */
bool validate_int_range(int value, int min, int max);

/* ========================================================================
 * 檔案系統驗證函式
 * ======================================================================== */

/**
 * @brief 驗證檔案名稱是否有效
 *
 * 檢查檔案名稱是否符合安全規範：
 * - 長度在 255 字元以內
 * - 不包含斜線等禁止字元
 * - 不以 ".." 開頭
 *
 * @param filename 檔案名稱
 * @return true 有效，false 無效
 */
bool validate_filename(const char *filename);

/**
 * @brief 驗證路徑長度
 *
 * @param path    路徑字串
 * @param max_len 最大允許長度（0 表示使用預設值 4096）
 * @return true 長度有效，false 超過限制
 */
bool validate_path_length(const char *path, size_t max_len);

#endif // VALIDATION_H
