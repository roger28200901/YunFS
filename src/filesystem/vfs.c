/**
 * @file vfs.c
 * @brief 虛擬檔案系統（VFS）模組實作
 *
 * 實作記憶體內的虛擬檔案系統，使用樹狀結構管理檔案與目錄。
 *
 * @author Yun
 * @date 2025
 */

#include "vfs.h"
#include "path.h"
#include "../security/sanitize.h"
#include "../security/validation.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

/* ========================================================================
 * 內部輔助函式宣告
 * ======================================================================== */

static vfs_node_t *create_node(const char *name, vfs_node_type_t type);
static void destroy_node(vfs_node_t *node);
static vfs_node_t *find_child(vfs_node_t *parent, const char *name);
static bool add_child(vfs_node_t *parent, vfs_node_t *child);
static bool remove_child(vfs_node_t *parent, vfs_node_t *child);
static vfs_node_t *resolve_path(vfs_t *vfs, const char *path, bool create_dirs);
static char **split_path(const char *path, size_t *count);

/* ========================================================================
 * VFS 生命週期函式實作
 * ======================================================================== */

/**
 * @brief 初始化虛擬檔案系統
 */
vfs_t *vfs_init(void) {
    vfs_t *vfs = (vfs_t *)safe_malloc(sizeof(vfs_t));
    if (vfs == NULL) {
        return NULL;
    }
    
    /* 建立根目錄節點 */
    vfs->root = create_node("/", VFS_DIR);
    if (vfs->root == NULL) {
        safe_free(vfs);
        return NULL;
    }
    
    vfs->root->parent = NULL;
    vfs->total_nodes = 1;
    vfs->total_size = 0;
    
    return vfs;
}

/**
 * @brief 銷毀虛擬檔案系統
 */
void vfs_destroy(vfs_t *vfs) {
    if (vfs == NULL) {
        return;
    }
    
    if (vfs->root != NULL) {
        destroy_node(vfs->root);
    }
    
    safe_free(vfs);
}

/* ========================================================================
 * 內部輔助函式實作
 * ======================================================================== */

/**
 * @brief 建立新節點
 *
 * @param name 節點名稱
 * @param type 節點類型
 * @return 新節點指標，失敗回傳 NULL
 */
static vfs_node_t *create_node(const char *name, vfs_node_type_t type) {
    if (name == NULL) {
        error_set(ERR_INVALID_INPUT, "節點名稱為 NULL");
        return NULL;
    }
    
    vfs_node_t *node = (vfs_node_t *)safe_malloc(sizeof(vfs_node_t));
    if (node == NULL) {
        return NULL;
    }
    
    node->name = safe_strdup(name);
    if (node->name == NULL) {
        safe_free(node);
        return NULL;
    }
    
    node->type = type;
    node->data = NULL;
    node->size = 0;
    node->mtime = time(NULL);
    node->ctime = node->mtime;
    node->parent = NULL;
    node->children = NULL;
    node->next = NULL;
    
    return node;
}

/**
 * @brief 遞迴銷毀節點及其子節點
 *
 * @param node 要銷毀的節點
 */
static void destroy_node(vfs_node_t *node) {
    if (node == NULL) {
        return;
    }
    
    /* 遞迴銷毀所有子節點 */
    vfs_node_t *child = node->children;
    while (child != NULL) {
        vfs_node_t *next = child->next;
        destroy_node(child);
        child = next;
    }
    
    /* 安全清除並釋放檔案資料 */
    if (node->type == VFS_FILE && node->data != NULL) {
        secure_zero(node->data, node->size);
        safe_free(node->data);
    }
    
    safe_free(node->name);
    safe_free(node);
}

/**
 * @brief 在父節點中尋找指定名稱的子節點
 *
 * @param parent 父節點
 * @param name   要尋找的名稱
 * @return 找到的子節點，未找到回傳 NULL
 */
static vfs_node_t *find_child(vfs_node_t *parent, const char *name) {
    if (parent == NULL || name == NULL || parent->type != VFS_DIR) {
        return NULL;
    }
    
    vfs_node_t *child = parent->children;
    while (child != NULL) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next;
    }
    
    return NULL;
}

/**
 * @brief 將子節點加入父節點
 *
 * @param parent 父節點
 * @param child  要加入的子節點
 * @return true 成功，false 失敗（已存在同名節點）
 */
static bool add_child(vfs_node_t *parent, vfs_node_t *child) {
    if (parent == NULL || child == NULL || parent->type != VFS_DIR) {
        return false;
    }
    
    /* 檢查是否已存在同名節點 */
    if (find_child(parent, child->name) != NULL) {
        error_set(ERR_INVALID_INPUT, "節點已存在: %s", child->name);
        return false;
    }
    
    /* 將子節點插入鏈結串列頭部 */
    child->parent = parent;
    child->next = parent->children;
    parent->children = child;
    
    if (parent->type == VFS_DIR) {
        parent->size++;
    }
    
    parent->mtime = time(NULL);
    
    return true;
}

/**
 * @brief 從父節點移除子節點
 *
 * @param parent 父節點
 * @param child  要移除的子節點
 * @return true 成功，false 失敗
 */
static bool remove_child(vfs_node_t *parent, vfs_node_t *child) {
    if (parent == NULL || child == NULL || parent->type != VFS_DIR) {
        return false;
    }
    
    /* 處理鏈結串列移除 */
    if (parent->children == child) {
        parent->children = child->next;
    } else {
        vfs_node_t *curr = parent->children;
        while (curr != NULL && curr->next != child) {
            curr = curr->next;
        }
        if (curr != NULL) {
            curr->next = child->next;
        } else {
            return false;
        }
    }
    
    child->next = NULL;
    child->parent = NULL;
    
    if (parent->type == VFS_DIR) {
        parent->size--;
    }
    
    parent->mtime = time(NULL);
    
    return true;
}

/**
 * @brief 分割路徑為組件陣列
 *
 * @param path  路徑字串
 * @param count 輸出參數，組件數量
 * @return 組件字串陣列（需由呼叫者釋放），失敗回傳 NULL
 */
static char **split_path(const char *path, size_t *count) {
    if (path == NULL || count == NULL) {
        return NULL;
    }
    
    /* 規範化路徑 */
    char *normalized = normalize_path(path);
    if (normalized == NULL) {
        return NULL;
    }
    
    /* 計算路徑組件數量 */
    size_t components = 0;
    for (size_t i = 0; normalized[i] != '\0'; i++) {
        if (normalized[i] == '/' && i > 0 && normalized[i-1] != '/') {
            components++;
        }
    }
    if (normalized[strlen(normalized)-1] != '/') {
        components++;
    }
    
    char **result = (char **)safe_malloc(sizeof(char *) * (components + 1));
    if (result == NULL) {
        safe_free(normalized);
        return NULL;
    }
    
    /* 使用 strtok 分割路徑 */
    size_t idx = 0;
    char *token = strtok(normalized, "/");
    while (token != NULL && idx < components) {
        result[idx++] = safe_strdup(token);
        token = strtok(NULL, "/");
    }
    result[idx] = NULL;
    *count = idx;
    
    safe_free(normalized);
    return result;
}

/**
 * @brief 解析路徑並回傳對應節點
 *
 * @param vfs         VFS 實例
 * @param path        路徑字串
 * @param create_dirs 若為 true，自動建立不存在的中間目錄
 * @return 解析到的節點，失敗回傳 NULL
 */
static vfs_node_t *resolve_path(vfs_t *vfs, const char *path, bool create_dirs) {
    if (vfs == NULL || path == NULL || vfs->root == NULL) {
        return NULL;
    }
    
    /* 規範化路徑 */
    char *normalized = normalize_path(path);
    if (normalized == NULL) {
        return NULL;
    }
    
    /* 分割路徑為組件 */
    size_t count = 0;
    char **components = split_path(normalized, &count);
    safe_free(normalized);
    
    if (components == NULL) {
        return NULL;
    }
    
    /* 從根節點開始遍歷 */
    vfs_node_t *current = vfs->root;
    
    for (size_t i = 0; i < count; i++) {
        if (components[i] == NULL || strlen(components[i]) == 0) {
            continue;
        }
        
        vfs_node_t *child = find_child(current, components[i]);
        
        if (child == NULL) {
            if (create_dirs && i < count - 1) {
                /* 自動建立中間目錄 */
                child = create_node(components[i], VFS_DIR);
                if (child == NULL) {
                    for (size_t j = 0; j < count; j++) {
                        safe_free(components[j]);
                    }
                    safe_free(components);
                    return NULL;
                }
                add_child(current, child);
            } else {
                /* 路徑不存在 */
                for (size_t j = 0; j < count; j++) {
                    safe_free(components[j]);
                }
                safe_free(components);
                return NULL;
            }
        }
        
        current = child;
    }
    
    /* 釋放組件陣列 */
    for (size_t i = 0; i < count; i++) {
        safe_free(components[i]);
    }
    safe_free(components);
    
    return current;
}

/* ========================================================================
 * 節點操作函式實作
 * ======================================================================== */

/**
 * @brief 建立檔案
 */
vfs_node_t *vfs_create_file(vfs_t *vfs, const char *path, const void *data, size_t size) {
    if (vfs == NULL || path == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return NULL;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return NULL;
    }
    
    /* 取得目錄與檔案名稱 */
    char *dir_path = path_get_dirname(path);
    char *filename = path_get_basename(path);
    
    if (dir_path == NULL || filename == NULL) {
        safe_free(dir_path);
        safe_free(filename);
        return NULL;
    }
    
    /* 解析父目錄（自動建立中間目錄） */
    vfs_node_t *parent = resolve_path(vfs, dir_path, true);
    safe_free(dir_path);
    
    if (parent == NULL || parent->type != VFS_DIR) {
        safe_free(filename);
        error_set(ERR_FILE_NOT_FOUND, "父目錄不存在: %s", path);
        return NULL;
    }
    
    /* 檢查檔案是否已存在 */
    vfs_node_t *existing = find_child(parent, filename);
    if (existing != NULL) {
        safe_free(filename);
        error_set(ERR_INVALID_INPUT, "檔案已存在: %s", path);
        return NULL;
    }
    
    /* 建立檔案節點 */
    vfs_node_t *file = create_node(filename, VFS_FILE);
    safe_free(filename);
    
    if (file == NULL) {
        return NULL;
    }
    
    /* 複製檔案內容 */
    if (size > 0 && data != NULL) {
        file->data = safe_malloc(size);
        if (file->data == NULL) {
            destroy_node(file);
            return NULL;
        }
        memcpy(file->data, data, size);
        file->size = size;
    }
    
    /* 加入父目錄 */
    if (!add_child(parent, file)) {
        destroy_node(file);
        return NULL;
    }
    
    vfs->total_nodes++;
    vfs->total_size += size;
    
    return file;
}

/**
 * @brief 建立目錄
 */
vfs_node_t *vfs_create_dir(vfs_t *vfs, const char *path) {
    if (vfs == NULL || path == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return NULL;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return NULL;
    }
    
    /* 檢查節點是否已存在 */
    vfs_node_t *node = resolve_path(vfs, path, false);
    if (node != NULL) {
        error_set(ERR_INVALID_INPUT, "節點已存在: %s", path);
        return NULL;
    }
    
    /* 取得目錄路徑與名稱 */
    char *dir_path = path_get_dirname(path);
    char *dirname = path_get_basename(path);
    
    if (dir_path == NULL || dirname == NULL) {
        safe_free(dir_path);
        safe_free(dirname);
        return NULL;
    }
    
    /* 解析父目錄 */
    vfs_node_t *parent = resolve_path(vfs, dir_path, true);
    safe_free(dir_path);
    
    if (parent == NULL || parent->type != VFS_DIR) {
        safe_free(dirname);
        error_set(ERR_FILE_NOT_FOUND, "父目錄不存在: %s", path);
        return NULL;
    }
    
    /* 建立目錄節點 */
    vfs_node_t *dir = create_node(dirname, VFS_DIR);
    safe_free(dirname);
    
    if (dir == NULL) {
        return NULL;
    }
    
    /* 加入父目錄 */
    if (!add_child(parent, dir)) {
        destroy_node(dir);
        return NULL;
    }
    
    vfs->total_nodes++;
    
    return dir;
}

/**
 * @brief 尋找節點
 */
vfs_node_t *vfs_find_node(vfs_t *vfs, const char *path) {
    if (vfs == NULL || path == NULL) {
        return NULL;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return NULL;
    }
    
    return resolve_path(vfs, path, false);
}

/**
 * @brief 刪除節點
 */
bool vfs_delete_node(vfs_t *vfs, const char *path) {
    if (vfs == NULL || path == NULL) {
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(path)) {
        return false;
    }
    
    /* 禁止刪除根目錄 */
    if (strcmp(path, "/") == 0) {
        error_set(ERR_PERMISSION, "不能刪除根目錄");
        return false;
    }
    
    vfs_node_t *node = resolve_path(vfs, path, false);
    if (node == NULL) {
        error_set(ERR_FILE_NOT_FOUND, "節點不存在: %s", path);
        return false;
    }
    
    if (node->parent == NULL) {
        error_set(ERR_PERMISSION, "不能刪除根節點");
        return false;
    }
    
    /* 從父節點移除 */
    remove_child(node->parent, node);
    
    /* 更新統計資訊 */
    if (node->type == VFS_FILE) {
        vfs->total_size -= node->size;
    }
    vfs->total_nodes--;
    
    /* 銷毀節點（含子節點） */
    destroy_node(node);
    
    return true;
}

/**
 * @brief 重新命名節點
 */
bool vfs_rename_node(vfs_t *vfs, const char *old_path, const char *new_path) {
    if (vfs == NULL || old_path == NULL || new_path == NULL) {
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(old_path) || is_path_traversal(new_path)) {
        return false;
    }
    
    vfs_node_t *node = resolve_path(vfs, old_path, false);
    if (node == NULL) {
        error_set(ERR_FILE_NOT_FOUND, "節點不存在: %s", old_path);
        return false;
    }
    
    char *new_name = path_get_basename(new_path);
    if (new_name == NULL) {
        return false;
    }
    
    /* 檢查新名稱是否已存在 */
    if (node->parent != NULL) {
        if (find_child(node->parent, new_name) != NULL) {
            safe_free(new_name);
            error_set(ERR_INVALID_INPUT, "目標名稱已存在");
            return false;
        }
    }
    
    /* 更新名稱 */
    safe_free(node->name);
    node->name = new_name;
    node->mtime = time(NULL);
    
    return true;
}

/**
 * @brief 移動節點
 */
bool vfs_move_node(vfs_t *vfs, const char *src_path, const char *dst_path) {
    if (vfs == NULL || src_path == NULL || dst_path == NULL) {
        return false;
    }
    
    /* 安全性檢查：路徑遍歷攻擊 */
    if (is_path_traversal(src_path) || is_path_traversal(dst_path)) {
        return false;
    }
    
    vfs_node_t *src_node = resolve_path(vfs, src_path, false);
    if (src_node == NULL) {
        error_set(ERR_FILE_NOT_FOUND, "源節點不存在: %s", src_path);
        return false;
    }
    
    char *dst_dir_path = path_get_dirname(dst_path);
    char *dst_name = path_get_basename(dst_path);
    
    if (dst_dir_path == NULL || dst_name == NULL) {
        safe_free(dst_dir_path);
        safe_free(dst_name);
        return false;
    }
    
    vfs_node_t *dst_parent = resolve_path(vfs, dst_dir_path, true);
    safe_free(dst_dir_path);
    
    if (dst_parent == NULL || dst_parent->type != VFS_DIR) {
        safe_free(dst_name);
        error_set(ERR_FILE_NOT_FOUND, "目標目錄不存在");
        return false;
    }
    
    /* 檢查目標名稱是否已存在 */
    if (find_child(dst_parent, dst_name) != NULL) {
        safe_free(dst_name);
        error_set(ERR_INVALID_INPUT, "目標名稱已存在");
        return false;
    }
    
    /* 從原位置移除 */
    if (src_node->parent != NULL) {
        remove_child(src_node->parent, src_node);
    }
    
    /* 更新名稱 */
    safe_free(src_node->name);
    src_node->name = dst_name;
    
    /* 加入新位置 */
    if (!add_child(dst_parent, src_node)) {
        safe_free(dst_name);
        return false;
    }
    
    src_node->mtime = time(NULL);
    
    return true;
}

/* ========================================================================
 * 檔案內容操作函式實作
 * ======================================================================== */

/**
 * @brief 讀取檔案內容
 */
void *vfs_read_file(vfs_node_t *node, size_t *size) {
    if (node == NULL || node->type != VFS_FILE) {
        error_set(ERR_INVALID_INPUT, "無效的檔案節點");
        return NULL;
    }
    
    if (size != NULL) {
        *size = node->size;
    }
    
    if (node->data == NULL || node->size == 0) {
        return NULL;
    }
    
    /* 回傳內容副本 */
    void *data = safe_malloc(node->size);
    if (data == NULL) {
        return NULL;
    }
    
    memcpy(data, node->data, node->size);
    return data;
}

/**
 * @brief 寫入檔案內容
 */
bool vfs_write_file(vfs_node_t *node, const void *data, size_t size) {
    if (node == NULL || node->type != VFS_FILE) {
        error_set(ERR_INVALID_INPUT, "無效的檔案節點");
        return false;
    }
    
    /* 安全清除並釋放舊資料 */
    if (node->data != NULL) {
        secure_zero(node->data, node->size);
        safe_free(node->data);
    }
    
    /* 配置並複製新資料 */
    if (size > 0 && data != NULL) {
        node->data = safe_malloc(size);
        if (node->data == NULL) {
            node->size = 0;
            return false;
        }
        memcpy(node->data, data, size);
    } else {
        node->data = NULL;
    }
    
    node->size = size;
    node->mtime = time(NULL);
    
    return true;
}

/* ========================================================================
 * 目錄操作函式實作
 * ======================================================================== */

/**
 * @brief 列出目錄內容
 */
vfs_node_t **vfs_list_dir(vfs_node_t *dir, size_t *count) {
    if (dir == NULL || dir->type != VFS_DIR || count == NULL) {
        error_set(ERR_INVALID_INPUT, "無效的目錄節點");
        return NULL;
    }
    
    /* 計算子節點數量 */
    size_t num_children = 0;
    vfs_node_t *child = dir->children;
    while (child != NULL) {
        num_children++;
        child = child->next;
    }
    
    if (num_children == 0) {
        *count = 0;
        return NULL;
    }
    
    /* 配置指標陣列 */
    vfs_node_t **result = (vfs_node_t **)safe_malloc(sizeof(vfs_node_t *) * num_children);
    if (result == NULL) {
        return NULL;
    }
    
    /* 填入子節點指標 */
    size_t idx = 0;
    child = dir->children;
    while (child != NULL) {
        result[idx++] = child;
        child = child->next;
    }
    
    *count = num_children;
    return result;
}

/**
 * @brief 取得節點的完整路徑
 */
char *vfs_get_path(vfs_node_t *node) {
    if (node == NULL) {
        return NULL;
    }
    
    /* 配置初始緩衝區 */
    size_t capacity = 256;
    char *path = (char *)safe_malloc(capacity);
    if (path == NULL) {
        return NULL;
    }
    
    path[0] = '\0';
    
    /* 收集從節點到根的路徑組件 */
    vfs_node_t *components[64];
    size_t depth = 0;
    vfs_node_t *curr = node;
    
    while (curr != NULL && depth < 64) {
        components[depth++] = curr;
        curr = curr->parent;
    }
    
    /* 反向建構路徑字串 */
    for (size_t i = depth; i > 0; i--) {
        size_t idx = i - 1;
        size_t len = strlen(path);
        size_t name_len = strlen(components[idx]->name);
        
        /* 必要時擴展緩衝區 */
        if (len + name_len + 2 > capacity) {
            capacity = (len + name_len + 2) * 2;
            char *new_path = (char *)safe_realloc(path, capacity);
            if (new_path == NULL) {
                safe_free(path);
                return NULL;
            }
            path = new_path;
        }
        
        if (idx == depth - 1 || (idx < depth - 1 && components[idx]->name[0] != '/')) {
            strncat(path, components[idx]->name, name_len);
        }
        
        if (idx > 0 && components[idx]->name[0] != '/') {
            strncat(path, "/", 1);
        }
    }
    
    if (strlen(path) == 0) {
        strncpy(path, "/", 2);
    }
    
    return path;
}
