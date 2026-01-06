/**
 * @file memory.c
 * @brief 安全記憶體管理模組實作
 *
 * 實作安全的記憶體配置、釋放與追蹤功能。
 *
 * @author Yun
 * @date 2025
 */

#include "memory.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef DEBUG
#include <stdio.h>
#include <stdint.h>

/* ============================================================================
 * Debug 模式記憶體追蹤結構
 * ============================================================================ */

/**
 * @brief 記憶體區塊追蹤結構
 */
typedef struct mem_block {
    void *ptr;              /**< 記憶體指標 */
    size_t size;            /**< 配置大小 */
    const char *file;       /**< 配置來源檔案 */
    int line;               /**< 配置來源行號 */
    struct mem_block *next; /**< 鏈結串列下一節點 */
} mem_block_t;

/** 記憶體區塊追蹤鏈結串列頭 */
static mem_block_t *g_mem_blocks = NULL;

/** 總配置量 */
static size_t g_total_allocated = 0;

/** 總釋放量 */
static size_t g_total_freed = 0;
#endif

/* ============================================================================
 * 安全記憶體配置函式
 * ============================================================================ */

/**
 * @brief 安全的記憶體配置
 */
void *safe_malloc(size_t size) {
    if (size == 0) {
        error_set(ERR_INVALID_INPUT, "嘗試分配 0 大小的記憶體");
        return NULL;
    }
    
    void *ptr = malloc(size);
    if (ptr == NULL) {
        error_set(ERR_MEMORY, "記憶體分配失敗: %s", strerror(errno));
        return NULL;
    }
    
    /* 初始化為零（安全措施） */
    memset(ptr, 0, size);
    return ptr;
}

/**
 * @brief 安全的陣列記憶體配置
 */
void *safe_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        error_set(ERR_INVALID_INPUT, "嘗試分配 0 大小的記憶體");
        return NULL;
    }
    
    /* 檢查乘法溢位 */
    if (nmemb > SIZE_MAX / size) {
        error_set(ERR_MEMORY, "記憶體分配大小溢出");
        return NULL;
    }
    
    void *ptr = calloc(nmemb, size);
    if (ptr == NULL) {
        error_set(ERR_MEMORY, "記憶體分配失敗: %s", strerror(errno));
        return NULL;
    }
    
    return ptr;
}

/**
 * @brief 安全的記憶體重新配置
 */
void *safe_realloc(void *ptr, size_t size) {
    if (size == 0) {
        error_set(ERR_INVALID_INPUT, "嘗試重新分配 0 大小的記憶體");
        return NULL;
    }
    
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size > 0) {
        error_set(ERR_MEMORY, "記憶體重新分配失敗: %s", strerror(errno));
        return NULL;
    }
    
    return new_ptr;
}

/**
 * @brief 安全的記憶體釋放
 */
void safe_free(void *ptr) {
    if (ptr != NULL) {
        free(ptr);
    }
}

/* ============================================================================
 * 安全字串操作
 * ============================================================================ */

/**
 * @brief 安全的字串複製
 */
char *safe_strdup(const char *s) {
    if (s == NULL) {
        error_set(ERR_INVALID_INPUT, "嘗試複製 NULL 字串");
        return NULL;
    }
    
    size_t len = strlen(s);
    char *dup = (char *)safe_malloc(len + 1);
    if (dup == NULL) {
        return NULL;
    }
    
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

/**
 * @brief 安全的限定長度字串複製
 */
char *safe_strndup(const char *s, size_t n) {
    if (s == NULL) {
        error_set(ERR_INVALID_INPUT, "嘗試複製 NULL 字串");
        return NULL;
    }
    
    size_t len = strnlen(s, n);
    char *dup = (char *)safe_malloc(len + 1);
    if (dup == NULL) {
        return NULL;
    }
    
    memcpy(dup, s, len);
    dup[len] = '\0';
    return dup;
}

/* ============================================================================
 * 安全性函式
 * ============================================================================ */

/**
 * @brief 安全的記憶體清零
 */
void secure_zero(void *ptr, size_t size) {
    if (ptr == NULL || size == 0) {
        return;
    }
    
    /* 使用 volatile 防止編譯器最佳化移除清零操作 */
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (size--) {
        *p++ = 0;
    }
}

/* ============================================================================
 * Debug 模式記憶體追蹤
 * ============================================================================ */

#ifdef DEBUG
/**
 * @brief 追蹤記憶體配置
 */
void memory_track_alloc(void *ptr, size_t size, const char *file, int line) {
    if (ptr == NULL) return;
    
    mem_block_t *block = (mem_block_t *)malloc(sizeof(mem_block_t));
    if (block == NULL) return;
    
    block->ptr = ptr;
    block->size = size;
    block->file = file;
    block->line = line;
    block->next = g_mem_blocks;
    g_mem_blocks = block;
    g_total_allocated += size;
}

/**
 * @brief 追蹤記憶體釋放
 */
void memory_track_free(void *ptr, const char *file, int line) {
    if (ptr == NULL) return;
    
    (void)file;  /* 避免未使用警告 */
    (void)line;
    
    mem_block_t *prev = NULL;
    mem_block_t *curr = g_mem_blocks;
    
    while (curr != NULL) {
        if (curr->ptr == ptr) {
            if (prev == NULL) {
                g_mem_blocks = curr->next;
            } else {
                prev->next = curr->next;
            }
            g_total_freed += curr->size;
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/**
 * @brief 印出記憶體使用統計
 */
void memory_print_stats(void) {
    printf("\n=== 記憶體統計 ===\n");
    printf("總分配: %zu 位元組\n", g_total_allocated);
    printf("總釋放: %zu 位元組\n", g_total_freed);
    printf("未釋放: %zu 位元組\n", g_total_allocated - g_total_freed);
    
    if (g_mem_blocks != NULL) {
        printf("\n未釋放的記憶體區塊:\n");
        mem_block_t *curr = g_mem_blocks;
        while (curr != NULL) {
            printf("  %p: %zu 位元組 [%s:%d]\n", 
                   curr->ptr, curr->size, curr->file, curr->line);
            curr = curr->next;
        }
    }
    printf("==================\n\n");
}
#endif
