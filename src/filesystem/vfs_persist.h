/**
 * @file vfs_persist.h
 * @brief VFS 持久化模組標頭檔
 *
 * 本模組提供虛擬檔案系統的持久化功能，包含：
 * - 將 VFS 序列化並加密儲存到實體檔案
 * - 從加密檔案載入並還原 VFS
 *
 * 使用 ChaCha20 串流加密演算法保護資料安全。
 *
 * @author Yun
 * @date 2025
 */

#ifndef VFS_PERSIST_H
#define VFS_PERSIST_H

#include "vfs.h"
#include <stdbool.h>

/* ========================================================================
 * VFS 持久化函式
 * ======================================================================== */

/**
 * @brief 將 VFS 加密儲存到檔案
 *
 * 將整個虛擬檔案系統序列化後，使用 ChaCha20 加密並儲存。
 *
 * @param vfs      VFS 實例
 * @param filename 儲存的檔案名稱
 * @param key      加密密鑰字串
 * @return true 成功，false 失敗
 *
 * @note 檔案格式包含魔數、版本號與加密資料
 */
bool vfs_save_encrypted(vfs_t *vfs, const char *filename, const char *key);

/**
 * @brief 從加密檔案載入 VFS
 *
 * 讀取加密檔案，解密後還原虛擬檔案系統。
 *
 * @param filename 檔案名稱
 * @param key      解密密鑰字串
 * @return VFS 實例指標，失敗或密鑰錯誤回傳 NULL
 *
 * @note 若檔案不存在，會回傳新建立的空 VFS
 */
vfs_t *vfs_load_encrypted(const char *filename, const char *key);

#endif // VFS_PERSIST_H
