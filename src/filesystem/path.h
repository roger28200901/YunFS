/**
 * @file path.h
 * @brief 路徑處理模組標頭檔
 *
 * 本模組提供路徑字串的解析與處理功能，包含：
 * - 路徑分割（目錄與檔案名稱）
 * - 絕對路徑判斷
 * - 副檔名擷取
 *
 * @author Yun
 * @date 2025
 */

#ifndef PATH_H
#define PATH_H

#include <stdbool.h>
#include <stddef.h>

/* ========================================================================
 * 路徑解析函式
 * ======================================================================== */

/**
 * @brief 取得路徑的目錄部分
 *
 * 從完整路徑中擷取目錄部分，類似 POSIX dirname 函式。
 * 例如："/home/user/file.txt" -> "/home/user"
 *
 * @param path 完整路徑字串
 * @return 目錄路徑字串（需由呼叫者釋放），失敗回傳 NULL
 *
 * @note 若路徑不含斜線，回傳 "."
 * @note 若路徑為根目錄，回傳 "/"
 */
char *path_get_dirname(const char *path);

/**
 * @brief 取得路徑的檔案名稱部分
 *
 * 從完整路徑中擷取檔案名稱，類似 POSIX basename 函式。
 * 例如："/home/user/file.txt" -> "file.txt"
 *
 * @param path 完整路徑字串
 * @return 檔案名稱字串（需由呼叫者釋放），失敗回傳 NULL
 *
 * @note 若路徑不含斜線，回傳整個路徑
 */
char *path_get_basename(const char *path);

/**
 * @brief 檢查路徑是否為絕對路徑
 *
 * 判斷路徑是否以 '/' 開頭。
 *
 * @param path 路徑字串
 * @return true 為絕對路徑，false 為相對路徑或無效輸入
 */
bool path_is_absolute(const char *path);

/**
 * @brief 檢查路徑是否存在（於 VFS 中）
 *
 * @param path 路徑字串
 * @return true 路徑存在，false 不存在
 *
 * @note 此函式目前為預留介面，尚未實作
 */
bool path_exists(const char *path);

/**
 * @brief 分割路徑為目錄與檔案名稱
 *
 * 同時取得路徑的目錄部分與檔案名稱部分。
 *
 * @param path 完整路徑字串
 * @param dir  輸出參數，目錄路徑（需由呼叫者釋放）
 * @param file 輸出參數，檔案名稱（需由呼叫者釋放）
 *
 * @note 若 dir 或 file 為 NULL，則不輸出該部分
 */
void path_split(const char *path, char **dir, char **file);

/**
 * @brief 取得路徑的副檔名
 *
 * 從路徑中擷取副檔名（不含點號）。
 * 例如："/home/user/file.txt" -> "txt"
 *
 * @param path 路徑字串
 * @return 副檔名指標（指向原字串內部），無副檔名回傳 NULL
 *
 * @note 回傳值為指向原字串的指標，不需釋放
 */
const char *path_get_extension(const char *path);

#endif // PATH_H
