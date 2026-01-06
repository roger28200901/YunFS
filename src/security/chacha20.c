/**
 * @file chacha20.c
 * @brief ChaCha20 串流加密模組實作
 *
 * 實作 ChaCha20 對稱串流加密演算法。
 * 演算法規格參考 RFC 7539。
 *
 * @author Yun
 * @date 2025
 */

#include "chacha20.h"
#include "../utils/memory.h"
#include <string.h>
#include <stdint.h>

/* ========================================================================
 * ChaCha20 常數定義
 * ======================================================================== */

/** ChaCha20 初始化常數 "expand 32-byte k" */
#define CHACHA20_CONSTANT_0 0x61707865
#define CHACHA20_CONSTANT_1 0x3320646e
#define CHACHA20_CONSTANT_2 0x79622d32
#define CHACHA20_CONSTANT_3 0x6b206574

/* ========================================================================
 * 內部狀態與輔助函式
 * ======================================================================== */

/** ChaCha20 內部狀態（16 個 32 位元字） */
static uint32_t state[16];

/** 工作狀態緩衝區 */
static uint32_t working_state[16];

/**
 * @brief 32 位元左旋轉
 *
 * @param x 輸入值
 * @param n 旋轉位數
 * @return 旋轉後的值
 */
static inline uint32_t rotl32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

/**
 * @brief ChaCha20 四分之一輪函式
 *
 * 執行 ChaCha20 的核心混合操作。
 *
 * @param a 狀態字指標
 * @param b 狀態字指標
 * @param c 狀態字指標
 * @param d 狀態字指標
 */
static void quarter_round(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    *a += *b; *d ^= *a; *d = rotl32(*d, 16);
    *c += *d; *b ^= *c; *b = rotl32(*b, 12);
    *a += *b; *d ^= *a; *d = rotl32(*d, 8);
    *c += *d; *b ^= *c; *b = rotl32(*b, 7);
}

/**
 * @brief 從位元組陣列載入 32 位元字（little-endian）
 *
 * @param b 位元組陣列指標
 * @return 32 位元字
 */
static uint32_t load32_le(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | 
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

/**
 * @brief 產生一個 ChaCha20 區塊
 *
 * 執行 20 輪（10 次雙輪）混合操作，產生 64 位元組的密鑰流。
 *
 * @param output 輸出的 16 個 32 位元字
 */
static void chacha20_block(uint32_t *output) {
    /* 複製狀態到工作緩衝區 */
    memcpy(working_state, state, sizeof(state));
    
    /* 執行 10 次雙輪（共 20 輪） */
    for (int i = 0; i < 10; i++) {
        /* 行輪 */
        quarter_round(&working_state[0], &working_state[4], &working_state[8], &working_state[12]);
        quarter_round(&working_state[1], &working_state[5], &working_state[9], &working_state[13]);
        quarter_round(&working_state[2], &working_state[6], &working_state[10], &working_state[14]);
        quarter_round(&working_state[3], &working_state[7], &working_state[11], &working_state[15]);
        
        /* 對角線輪 */
        quarter_round(&working_state[0], &working_state[5], &working_state[10], &working_state[15]);
        quarter_round(&working_state[1], &working_state[6], &working_state[11], &working_state[12]);
        quarter_round(&working_state[2], &working_state[7], &working_state[8], &working_state[13]);
        quarter_round(&working_state[3], &working_state[4], &working_state[9], &working_state[14]);
    }
    
    /* 將工作狀態加回原始狀態 */
    for (int i = 0; i < 16; i++) {
        output[i] = working_state[i] + state[i];
    }
    
    /* 遞增計數器 */
    state[12]++;
    if (state[12] == 0) {
        state[13]++;  /* 處理溢位 */
    }
}

/* ========================================================================
 * 公開函式實作
 * ======================================================================== */

/**
 * @brief 初始化 ChaCha20 加密狀態
 */
void chacha20_init(const uint8_t *key, const uint8_t *nonce, uint32_t counter) {
    /* 設定初始化常數 */
    state[0] = CHACHA20_CONSTANT_0;
    state[1] = CHACHA20_CONSTANT_1;
    state[2] = CHACHA20_CONSTANT_2;
    state[3] = CHACHA20_CONSTANT_3;
    
    /* 載入 256 位元密鑰（8 個 32 位元字） */
    for (int i = 0; i < 8; i++) {
        state[4 + i] = load32_le(key + i * 4);
    }
    
    /* 設定計數器 */
    state[12] = counter;
    
    /* 載入 96 位元 nonce（3 個 32 位元字） */
    state[13] = load32_le(nonce);
    state[14] = load32_le(nonce + 4);
    state[15] = load32_le(nonce + 8);
}

/**
 * @brief 加密或解密資料
 */
void chacha20_encrypt(const uint8_t *input, uint8_t *output, size_t len) {
    uint32_t keystream[16];
    size_t pos = 0;
    
    while (pos < len) {
        /* 產生 64 位元組的密鑰流 */
        chacha20_block(keystream);
        
        /* 使用 XOR 進行加密/解密 */
        size_t block_pos = 0;
        while (block_pos < 64 && pos < len) {
            output[pos] = input[pos] ^ ((uint8_t *)keystream)[block_pos];
            pos++;
            block_pos++;
        }
    }
}

/**
 * @brief 從密鑰字串衍生 32 位元組密鑰
 */
void chacha20_derive_key(const char *key_str, uint8_t *key) {
    memset(key, 0, 32);
    size_t key_len = strlen(key_str);
    
    /* 簡單的密鑰擴展（非標準 KDF） */
    for (size_t i = 0; i < 32; i++) {
        key[i] = (uint8_t)(key_str[i % key_len] ^ (i * 7));
    }
    
    /* 額外的混合操作增加擴散性 */
    for (int i = 0; i < 32; i++) {
        key[i] = key[i] ^ key[(i + 1) % 32];
        key[i] = (key[i] << 1) | (key[i] >> 7);
    }
}

/**
 * @brief 使用密鑰字串進行加密或解密
 */
void chacha20_encrypt_with_key(const char *key_str, const uint8_t *nonce, 
                                const uint8_t *input, uint8_t *output, size_t len) {
    uint8_t key[32];
    chacha20_derive_key(key_str, key);
    chacha20_init(key, nonce, 0);
    chacha20_encrypt(input, output, len);
}
