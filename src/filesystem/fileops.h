/**
 * @file fileops.h
 * @brief 檔案操作模組標頭檔
 *
 * 本模組提供安全的實體檔案系統操作封裝，包含：
 * - 檔案開啟、讀取、寫入
 * - 檔案複製、移動、刪除
 * - 檔案存在性與權限檢查
 *
 * 所有操作皆包含路徑驗證與安全性檢查。
 *
 * @author Yun
 * @date 2025
 */

#ifndef FILEOPS_H
#define FILEOPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* ========================================================================
 * 檔案開啟與讀寫函式
 * ======================================================================== */

/**
 * @brief 安全地開啟檔案
 *
 * 開啟檔案前會進行路徑驗證與安全性檢查。
 *
 * @param path 檔案路徑
 * @param mode 開啟模式（如 "rb", "wb"）
 * @return FILE 指標，失敗回傳 NULL
 *
 * @note 會檢查路徑遍歷攻擊
 */
FILE *fileops_open(const char *path, const char *mode);

/**
 * @brief 安全地讀取檔案內容
 *
 * 讀取整個檔案內容到記憶體緩衝區。
 *
 * @param path 檔案路徑
 * @param data 輸出參數，檔案內容指標（需由呼叫者釋放）
 * @param size 輸出參數，檔案大小
 * @return true 成功，false 失敗
 *
 * @note 檔案大小限制為 100MB
 */
bool fileops_read(const char *path, void **data, size_t *size);

/**
 * @brief 安全地寫入檔案內容
 *
 * 將資料寫入檔案（覆寫模式）。
 *
 * @param path 檔案路徑
 * @param data 要寫入的資料
 * @param size 資料大小
 * @return true 成功，false 失敗
 */
bool fileops_write(const char *path, const void *data, size_t size);

/* ========================================================================
 * 檔案管理函式
 * ======================================================================== */

/**
 * @brief 安全地複製檔案
 *
 * 將來源檔案複製到目標路徑。
 *
 * @param src 來源檔案路徑
 * @param dst 目標檔案路徑
 * @return true 成功，false 失敗
 */
bool fileops_copy(const char *src, const char *dst);

/**
 * @brief 安全地移動檔案
 *
 * 將檔案移動到新位置，優先使用原子操作（rename）。
 *
 * @param src 來源檔案路徑
 * @param dst 目標檔案路徑
 * @return true 成功，false 失敗
 *
 * @note 若 rename 失敗，會改用複製後刪除的方式
 */
bool fileops_move(const char *src, const char *dst);

/**
 * @brief 安全地刪除檔案
 *
 * @param path 檔案路徑
 * @return true 成功，false 失敗
 */
bool fileops_delete(const char *path);

/* ========================================================================
 * 檔案資訊函式
 * ======================================================================== */

/**
 * @brief 檢查檔案是否存在
 *
 * @param path 檔案路徑
 * @return true 存在，false 不存在或無法存取
 */
bool fileops_exists(const char *path);

/**
 * @brief 取得檔案大小
 *
 * @param path 檔案路徑
 * @param size 輸出參數，檔案大小（位元組）
 * @return true 成功，false 失敗
 */
bool fileops_get_size(const char *path, size_t *size);

/**
 * @brief 檢查檔案權限
 *
 * @param path 檔案路徑
 * @param mode 權限模式字串（如 "r", "w", "x", "rw"）
 * @return true 具有指定權限，false 無權限
 */
bool fileops_check_permission(const char *path, const char *mode);

#endif // FILEOPS_H
