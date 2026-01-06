/**
 * @file vfs.h
 * @brief 虛擬檔案系統（VFS）模組標頭檔
 *
 * 本模組實作記憶體內的虛擬檔案系統，提供：
 * - 檔案與目錄的建立、刪除、重新命名
 * - 檔案內容的讀取與寫入
 * - 目錄內容列表
 * - 樹狀結構的節點管理
 *
 * @author Yun
 * @date 2025
 */

#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/* ========================================================================
 * 型別定義
 * ======================================================================== */

/**
 * @brief VFS 節點類型列舉
 */
typedef enum {
    VFS_FILE,   /**< 檔案節點 */
    VFS_DIR     /**< 目錄節點 */
} vfs_node_type_t;

/**
 * @brief VFS 節點結構
 *
 * 表示虛擬檔案系統中的單一節點（檔案或目錄）。
 * 使用鏈結串列管理子節點與兄弟節點。
 */
typedef struct vfs_node {
    char *name;                    /**< 節點名稱 */
    vfs_node_type_t type;          /**< 節點類型（檔案/目錄） */
    void *data;                    /**< 檔案內容（目錄為 NULL） */
    size_t size;                   /**< 大小（檔案：位元組數，目錄：子節點數） */
    time_t mtime;                  /**< 最後修改時間 */
    time_t ctime;                  /**< 建立時間 */
    struct vfs_node *parent;       /**< 父節點指標 */
    struct vfs_node *children;     /**< 第一個子節點（鏈結串列頭） */
    struct vfs_node *next;         /**< 下一個兄弟節點 */
} vfs_node_t;

/**
 * @brief VFS 檔案系統結構
 *
 * 管理整個虛擬檔案系統的根節點與統計資訊。
 */
typedef struct {
    vfs_node_t *root;              /**< 根目錄節點 */
    size_t total_nodes;            /**< 總節點數量 */
    size_t total_size;             /**< 總檔案大小（位元組） */
} vfs_t;

/* ========================================================================
 * VFS 生命週期函式
 * ======================================================================== */

/**
 * @brief 初始化虛擬檔案系統
 *
 * 建立新的 VFS 實例，包含空的根目錄。
 *
 * @return VFS 實例指標，失敗回傳 NULL
 */
vfs_t *vfs_init(void);

/**
 * @brief 銷毀虛擬檔案系統
 *
 * 釋放 VFS 及其所有節點佔用的記憶體。
 *
 * @param vfs VFS 實例指標
 */
void vfs_destroy(vfs_t *vfs);

/* ========================================================================
 * 節點操作函式
 * ======================================================================== */

/**
 * @brief 建立檔案
 *
 * 在指定路徑建立新檔案，並設定初始內容。
 *
 * @param vfs  VFS 實例
 * @param path 檔案完整路徑
 * @param data 初始檔案內容（可為 NULL）
 * @param size 內容大小（位元組）
 * @return 新建立的檔案節點，失敗回傳 NULL
 *
 * @note 若中間目錄不存在會自動建立
 */
vfs_node_t *vfs_create_file(vfs_t *vfs, const char *path, const void *data, size_t size);

/**
 * @brief 建立目錄
 *
 * 在指定路徑建立新目錄。
 *
 * @param vfs  VFS 實例
 * @param path 目錄完整路徑
 * @return 新建立的目錄節點，失敗回傳 NULL
 */
vfs_node_t *vfs_create_dir(vfs_t *vfs, const char *path);

/**
 * @brief 刪除節點
 *
 * 刪除指定路徑的檔案或目錄（含子節點）。
 *
 * @param vfs  VFS 實例
 * @param path 節點完整路徑
 * @return true 刪除成功，false 失敗
 *
 * @note 無法刪除根目錄
 */
bool vfs_delete_node(vfs_t *vfs, const char *path);

/**
 * @brief 尋找節點
 *
 * 根據路徑尋找對應的節點。
 *
 * @param vfs  VFS 實例
 * @param path 節點完整路徑
 * @return 找到的節點指標，未找到回傳 NULL
 */
vfs_node_t *vfs_find_node(vfs_t *vfs, const char *path);

/**
 * @brief 重新命名節點
 *
 * 變更節點的名稱（不移動位置）。
 *
 * @param vfs      VFS 實例
 * @param old_path 原始路徑
 * @param new_path 新路徑（僅使用檔案名稱部分）
 * @return true 成功，false 失敗
 */
bool vfs_rename_node(vfs_t *vfs, const char *old_path, const char *new_path);

/**
 * @brief 移動節點
 *
 * 將節點移動到新的位置。
 *
 * @param vfs      VFS 實例
 * @param src_path 來源路徑
 * @param dst_path 目標路徑
 * @return true 成功，false 失敗
 */
bool vfs_move_node(vfs_t *vfs, const char *src_path, const char *dst_path);

/* ========================================================================
 * 檔案內容操作函式
 * ======================================================================== */

/**
 * @brief 讀取檔案內容
 *
 * 取得檔案節點的內容副本。
 *
 * @param node 檔案節點指標
 * @param size 輸出參數，內容大小（可為 NULL）
 * @return 內容副本（需由呼叫者釋放），失敗回傳 NULL
 */
void *vfs_read_file(vfs_node_t *node, size_t *size);

/**
 * @brief 寫入檔案內容
 *
 * 覆寫檔案節點的內容。
 *
 * @param node 檔案節點指標
 * @param data 新內容
 * @param size 內容大小（位元組）
 * @return true 成功，false 失敗
 */
bool vfs_write_file(vfs_node_t *node, const void *data, size_t size);

/* ========================================================================
 * 目錄操作函式
 * ======================================================================== */

/**
 * @brief 列出目錄內容
 *
 * 取得目錄下所有子節點的陣列。
 *
 * @param dir   目錄節點指標
 * @param count 輸出參數，子節點數量
 * @return 子節點指標陣列（需由呼叫者釋放），失敗回傳 NULL
 *
 * @note 回傳的陣列需釋放，但陣列中的節點指標不需釋放
 */
vfs_node_t **vfs_list_dir(vfs_node_t *dir, size_t *count);

/**
 * @brief 取得節點的完整路徑
 *
 * 從節點向上追溯建構完整路徑字串。
 *
 * @param node 節點指標
 * @return 完整路徑字串（需由呼叫者釋放），失敗回傳 NULL
 */
char *vfs_get_path(vfs_node_t *node);

#endif // VFS_H
