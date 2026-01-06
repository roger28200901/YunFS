/**
 * @file vfs_persist.c
 * @brief VFS 持久化模組實作
 *
 * 實作虛擬檔案系統的序列化、加密儲存與載入功能。
 *
 * @author Yun
 * @date 2025
 */

#include "vfs_persist.h"
#include "vfs.h"
#include "../security/chacha20.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/** 檔案格式魔數，用於識別有效的 VFS 檔案 */
#define VFS_MAGIC "YUNVFS01"

/** 檔案格式版本號 */
#define VFS_VERSION 1

/* ========================================================================
 * 內部輔助函式宣告
 * ======================================================================== */

static size_t serialize_node(vfs_node_t *node, uint8_t *buffer, size_t buffer_size, size_t offset);
static vfs_node_t *deserialize_node(const uint8_t *buffer, size_t buffer_size, size_t *offset, vfs_node_t *parent);
static size_t calculate_serialized_size(vfs_node_t *node);

/* ========================================================================
 * 內部輔助函式實作
 * ======================================================================== */

/**
 * @brief 計算節點序列化後的大小
 *
 * 遞迴計算節點及其所有子節點序列化所需的位元組數。
 *
 * @param node 節點指標
 * @return 序列化所需的位元組數
 */
static size_t calculate_serialized_size(vfs_node_t *node) {
    if (node == NULL) {
        return sizeof(uint32_t);  /* NULL 標記 */
    }
    
    size_t size = sizeof(uint32_t);           /* 類型標記 */
    size += sizeof(uint32_t);                  /* 名稱長度 */
    size += strlen(node->name) + 1;            /* 名稱（含結尾） */
    size += sizeof(size_t);                    /* 資料大小 */
    size += sizeof(time_t) * 2;                /* mtime, ctime */
    
    if (node->type == VFS_FILE) {
        /* 檔案：只有在有資料時才計算大小 */
        if (node->data != NULL && node->size > 0) {
            size += node->size;                    /* 檔案內容 */
        }
    } else {
        /* 目錄：遞迴計算子節點 */
        size += sizeof(uint32_t);              /* 子節點數量 */
        vfs_node_t *child = node->children;
        while (child != NULL) {
            size += calculate_serialized_size(child);
            child = child->next;
        }
    }
    
    return size;
}

/**
 * @brief 序列化節點到緩衝區
 *
 * 將節點資料寫入二進位緩衝區，遞迴處理子節點。
 *
 * @param node        節點指標
 * @param buffer      目標緩衝區
 * @param buffer_size 緩衝區大小
 * @param offset      目前寫入位置
 * @return 新的寫入位置
 */
static size_t serialize_node(vfs_node_t *node, uint8_t *buffer, size_t buffer_size, size_t offset) {
    if (node == NULL) {
        if (offset + sizeof(uint32_t) > buffer_size) return offset;
        *(uint32_t *)(buffer + offset) = 0;  /* NULL 標記 */
        return offset + sizeof(uint32_t);
    }
    
    /* 寫入節點類型 */
    if (offset + sizeof(uint32_t) > buffer_size) return offset;
    /* 注意：vfs_node_type_t 目前 VFS_FILE == 0，與 NULL 標記衝突。
     * 因此序列化時統一寫入 (type + 1)，保留 0 給 NULL。 */
    *(uint32_t *)(buffer + offset) = (uint32_t)node->type + 1;
    offset += sizeof(uint32_t);
    
    /* 寫入名稱長度與名稱 */
    uint32_t name_len = (uint32_t)strlen(node->name);
    if (offset + sizeof(uint32_t) + name_len + 1 > buffer_size) return offset;
    *(uint32_t *)(buffer + offset) = name_len;
    offset += sizeof(uint32_t);
    memcpy(buffer + offset, node->name, name_len + 1);
    offset += name_len + 1;
    
    /* 寫入資料大小 */
    if (offset + sizeof(size_t) > buffer_size) return offset;
    *(size_t *)(buffer + offset) = node->size;
    offset += sizeof(size_t);
    
    /* 寫入時間戳記 */
    if (offset + sizeof(time_t) * 2 > buffer_size) return offset;
    *(time_t *)(buffer + offset) = node->mtime;
    offset += sizeof(time_t);
    *(time_t *)(buffer + offset) = node->ctime;
    offset += sizeof(time_t);
    
    if (node->type == VFS_FILE) {
        /* 寫入檔案內容 */
        if (node->data != NULL && node->size > 0) {
            if (offset + node->size > buffer_size) return offset;
            memcpy(buffer + offset, node->data, node->size);
            offset += node->size;
        }
    } else {
        /* 目錄：計算並寫入子節點數量 */
        uint32_t child_count = 0;
        vfs_node_t *child = node->children;
        while (child != NULL) {
            child_count++;
            child = child->next;
        }
        
        if (offset + sizeof(uint32_t) > buffer_size) return offset;
        *(uint32_t *)(buffer + offset) = child_count;
        offset += sizeof(uint32_t);
        
        /* 遞迴序列化子節點 */
        child = node->children;
        while (child != NULL) {
            offset = serialize_node(child, buffer, buffer_size, offset);
            child = child->next;
        }
    }
    
    return offset;
}

/**
 * @brief 從緩衝區反序列化節點
 *
 * 從二進位緩衝區讀取並重建節點結構，遞迴處理子節點。
 *
 * @param buffer      來源緩衝區
 * @param buffer_size 緩衝區大小
 * @param offset      目前讀取位置（會被更新）
 * @param parent      父節點指標
 * @return 重建的節點，失敗回傳 NULL
 */
static vfs_node_t *deserialize_node(const uint8_t *buffer, size_t buffer_size, size_t *offset, vfs_node_t *parent) {
    if (*offset + sizeof(uint32_t) > buffer_size) {
        error_set(ERR_INVALID_INPUT, "反序列化時緩衝區超出範圍 (offset=%zu, buffer_size=%zu)", *offset, buffer_size);
        return NULL;
    }
    
    /* 讀取類型標記 */
    uint32_t type_marker = *(uint32_t *)(buffer + *offset);
    *offset += sizeof(uint32_t);
    
    if (type_marker == 0) {
        return NULL;  /* NULL 節點 */
    }

    /* 反序列化時回復 type = (marker - 1) */
    type_marker -= 1;
    if (type_marker > (uint32_t)VFS_DIR) {
        error_set(ERR_INVALID_INPUT, "反序列化時節點類型無效: %u", (unsigned)(type_marker + 1));
        return NULL;
    }
    vfs_node_type_t type = (vfs_node_type_t)type_marker;
    
    /* 讀取名稱 */
    if (*offset + sizeof(uint32_t) > buffer_size) {
        error_set(ERR_INVALID_INPUT, "反序列化時名稱長度欄位超出緩衝區範圍 (offset=%zu, buffer_size=%zu)", *offset, buffer_size);
        return NULL;
    }
    uint32_t name_len = *(uint32_t *)(buffer + *offset);
    *offset += sizeof(uint32_t);
    
    if (*offset + name_len + 1 > buffer_size) {
        error_set(ERR_INVALID_INPUT, "反序列化時名稱長度超出緩衝區範圍 (name_len=%u, offset=%zu, buffer_size=%zu)", name_len, *offset, buffer_size);
        return NULL;
    }
    char *name = (char *)safe_malloc(name_len + 1);
    if (name == NULL) {
        error_set(ERR_MEMORY, "無法配置記憶體來讀取節點名稱");
        return NULL;
    }
    memcpy(name, buffer + *offset, name_len + 1);
    *offset += name_len + 1;
    
    /* 建立節點 */
    vfs_node_t *node = (vfs_node_t *)safe_malloc(sizeof(vfs_node_t));
    if (node == NULL) {
        safe_free(name);
        error_set(ERR_MEMORY, "無法配置記憶體來建立節點");
        return NULL;
    }
    
    node->name = name;
    node->type = type;
    node->parent = parent;
    node->children = NULL;
    node->next = NULL;
    
    /* 讀取大小 */
    if (*offset + sizeof(size_t) > buffer_size) {
        safe_free(node->name);
        safe_free(node);
        error_set(ERR_INVALID_INPUT, "反序列化時大小欄位超出緩衝區範圍");
        return NULL;
    }
    node->size = *(size_t *)(buffer + *offset);
    *offset += sizeof(size_t);
    
    /* 讀取時間戳記 */
    if (*offset + sizeof(time_t) * 2 > buffer_size) {
        safe_free(node->name);
        safe_free(node);
        error_set(ERR_INVALID_INPUT, "反序列化時時間戳記超出緩衝區範圍");
        return NULL;
    }
    node->mtime = *(time_t *)(buffer + *offset);
    *offset += sizeof(time_t);
    node->ctime = *(time_t *)(buffer + *offset);
    *offset += sizeof(time_t);
    
    if (type == VFS_FILE) {
        /* 讀取檔案內容 */
        if (node->size > 0) {
            if (*offset + node->size > buffer_size) {
                safe_free(node->name);
                if (node->data) safe_free(node->data);
                safe_free(node);
                error_set(ERR_INVALID_INPUT, "反序列化時檔案內容超出緩衝區範圍 (size=%zu, offset=%zu, buffer_size=%zu)", node->size, *offset, buffer_size);
                return NULL;
            }
            node->data = safe_malloc(node->size);
            if (node->data == NULL) {
                safe_free(node->name);
                safe_free(node);
                error_set(ERR_MEMORY, "無法配置記憶體來讀取檔案內容");
                return NULL;
            }
            memcpy(node->data, buffer + *offset, node->size);
            *offset += node->size;
        } else {
            node->data = NULL;
        }
    } else {
        /* 讀取子節點 */
        if (*offset + sizeof(uint32_t) > buffer_size) {
            safe_free(node->name);
            safe_free(node);
            error_set(ERR_INVALID_INPUT, "反序列化時子節點數量超出緩衝區範圍");
            return NULL;
        }
        uint32_t child_count = *(uint32_t *)(buffer + *offset);
        *offset += sizeof(uint32_t);
        
        vfs_node_t *prev_child = NULL;
        for (uint32_t i = 0; i < child_count; i++) {
            vfs_node_t *child = deserialize_node(buffer, buffer_size, offset, node);
            if (child == NULL) {
                /* 清理已建立的子節點 */
                vfs_node_t *curr = node->children;
                while (curr != NULL) {
                    vfs_node_t *next = curr->next;
                    safe_free(curr->name);
                    if (curr->type == VFS_FILE && curr->data) safe_free(curr->data);
                    safe_free(curr);
                    curr = next;
                }
                safe_free(node->name);
                safe_free(node);
                /* 錯誤訊息已經由 deserialize_node 設定，直接回傳 */
                return NULL;
            }
            
            if (prev_child == NULL) {
                node->children = child;
            } else {
                prev_child->next = child;
            }
            prev_child = child;
        }
        node->data = NULL;
    }
    
    return node;
}

/* ========================================================================
 * VFS 持久化函式實作
 * ======================================================================== */

/**
 * @brief 將 VFS 加密儲存到檔案
 */
bool vfs_save_encrypted(vfs_t *vfs, const char *filename, const char *key) {
    if (vfs == NULL || filename == NULL || key == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return false;
    }
    
    /* 計算序列化所需大小 */
    size_t serialized_size = sizeof(VFS_MAGIC) + sizeof(uint32_t) + calculate_serialized_size(vfs->root);
    
    /* 配置序列化緩衝區 */
    uint8_t *buffer = (uint8_t *)safe_malloc(serialized_size);
    if (buffer == NULL) {
        return false;
    }
    
    size_t offset = 0;
    
    /* 寫入魔數 */
    memcpy(buffer + offset, VFS_MAGIC, sizeof(VFS_MAGIC) - 1);
    offset += sizeof(VFS_MAGIC) - 1;
    
    /* 寫入版本號 */
    *(uint32_t *)(buffer + offset) = VFS_VERSION;
    offset += sizeof(uint32_t);
    
    /* 序列化根節點 */
    offset = serialize_node(vfs->root, buffer, serialized_size, offset);
    
    /* 配置加密緩衝區 */
    uint8_t *encrypted = (uint8_t *)safe_malloc(offset);
    if (encrypted == NULL) {
        safe_free(buffer);
        return false;
    }
    
    /* 設定 nonce（實際應用應使用隨機 nonce） */
    uint8_t nonce[12] = {0};
    memcpy(nonce, "yunhongisbest", 12);
    
    /* 執行加密 */
    chacha20_encrypt_with_key(key, nonce, buffer, encrypted, offset);
    
    /* 開啟檔案並寫入 */
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        safe_free(buffer);
        safe_free(encrypted);
        error_set(ERR_IO_ERROR, "無法打開檔案進行寫入");
        return false;
    }
    
    /* 寫入加密資料大小 */
    fwrite(&offset, sizeof(size_t), 1, file);
    /* 寫入加密資料 */
    fwrite(encrypted, 1, offset, file);
    
    fclose(file);
    
    /* 安全清除並釋放緩衝區 */
    secure_zero(buffer, serialized_size);
    secure_zero(encrypted, offset);
    safe_free(buffer);
    safe_free(encrypted);
    
    return true;
}

/**
 * @brief 從加密檔案載入 VFS
 */
vfs_t *vfs_load_encrypted(const char *filename, const char *key) {
    if (filename == NULL || key == NULL) {
        error_set(ERR_INVALID_INPUT, "參數為 NULL");
        return NULL;
    }
    
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        /* 檔案不存在，建立新的 VFS */
        return vfs_init();
    }
    
    /* 讀取加密資料大小 */
    size_t encrypted_size = 0;
    if (fread(&encrypted_size, sizeof(size_t), 1, file) != 1) {
        fclose(file);
        error_set(ERR_IO_ERROR, "無法讀取檔案大小");
        return NULL;
    }
    
    /* 讀取加密資料 */
    uint8_t *encrypted = (uint8_t *)safe_malloc(encrypted_size);
    if (encrypted == NULL) {
        fclose(file);
        error_set(ERR_MEMORY, "無法配置記憶體來讀取加密資料");
        return NULL;
    }
    
    if (fread(encrypted, 1, encrypted_size, file) != encrypted_size) {
        safe_free(encrypted);
        fclose(file);
        error_set(ERR_IO_ERROR, "無法讀取加密資料");
        return NULL;
    }
    
    fclose(file);
    
    /* 配置解密緩衝區 */
    uint8_t *decrypted = (uint8_t *)safe_malloc(encrypted_size);
    if (decrypted == NULL) {
        safe_free(encrypted);
        error_set(ERR_MEMORY, "無法配置記憶體來解密資料");
        return NULL;
    }
    
    /* 設定 nonce */
    uint8_t nonce[12] = {0};
    memcpy(nonce, "yunhongisbest", 12);
    
    /* 執行解密 */
    chacha20_encrypt_with_key(key, nonce, encrypted, decrypted, encrypted_size);
    
    safe_free(encrypted);
    
    /* 驗證魔數 */
    if (memcmp(decrypted, VFS_MAGIC, sizeof(VFS_MAGIC) - 1) != 0) {
        secure_zero(decrypted, encrypted_size);
        safe_free(decrypted);
        error_set(ERR_INVALID_INPUT, "無效的檔案格式或密鑰錯誤");
        return NULL;
    }
    
    size_t offset = sizeof(VFS_MAGIC) - 1;
    
    /* 讀取版本號 */
    uint32_t version = *(uint32_t *)(decrypted + offset);
    offset += sizeof(uint32_t);
    
    if (version != VFS_VERSION) {
        secure_zero(decrypted, encrypted_size);
        safe_free(decrypted);
        error_set(ERR_INVALID_INPUT, "不支持的檔案版本");
        return NULL;
    }
    
    /* 建立 VFS 結構 */
    vfs_t *vfs = (vfs_t *)safe_malloc(sizeof(vfs_t));
    if (vfs == NULL) {
        secure_zero(decrypted, encrypted_size);
        safe_free(decrypted);
        error_set(ERR_MEMORY, "無法配置記憶體來建立 VFS 結構");
        return NULL;
    }
    
    /* 反序列化根節點 */
    vfs->root = deserialize_node(decrypted, encrypted_size, &offset, NULL);
    if (vfs->root == NULL) {
        secure_zero(decrypted, encrypted_size);
        safe_free(decrypted);
        safe_free(vfs);
        error_set(ERR_INVALID_INPUT, "無法反序列化 VFS 資料（檔案可能損壞或格式錯誤）");
        return NULL;
    }
    
    /* 初始化統計資訊（簡化實作） */
    vfs->total_nodes = 1;
    vfs->total_size = 0;
    
    /* 安全清除並釋放緩衝區 */
    secure_zero(decrypted, encrypted_size);
    safe_free(decrypted);
    
    return vfs;
}
