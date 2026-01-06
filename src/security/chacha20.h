/**
 * @file chacha20.h
 * @brief ChaCha20 串流加密模組標頭檔
 *
 * 本模組實作 ChaCha20 對稱串流加密演算法，提供：
 * - 資料加密與解密（對稱操作）
 * - 密鑰衍生功能
 *
 * ChaCha20 是一種高效能的串流加密演算法，
 * 加密與解密使用相同的 XOR 操作。
 *
 * @author Yun
 * @date 2025
 */

#ifndef CHACHA20_H
#define CHACHA20_H

#include <stddef.h>
#include <stdint.h>

/* ========================================================================
 * ChaCha20 核心函式
 * ======================================================================== */

/**
 * @brief 初始化 ChaCha20 加密狀態
 *
 * 設定加密所需的密鑰、nonce 與計數器。
 *
 * @param key     32 位元組密鑰
 * @param nonce   12 位元組 nonce（一次性數值）
 * @param counter 初始計數器值（通常為 0）
 *
 * @note 使用相同密鑰時，每次加密必須使用不同的 nonce
 */
void chacha20_init(const uint8_t *key, const uint8_t *nonce, uint32_t counter);

/**
 * @brief 加密或解密資料
 *
 * ChaCha20 是對稱串流加密，加密與解密使用相同函式。
 * 對已加密資料再次呼叫此函式即可解密。
 *
 * @param input  輸入資料
 * @param output 輸出資料（大小必須與 input 相同）
 * @param len    資料長度（位元組）
 *
 * @note 呼叫前必須先執行 chacha20_init
 */
void chacha20_encrypt(const uint8_t *input, uint8_t *output, size_t len);

/* ========================================================================
 * 便利函式
 * ======================================================================== */

/**
 * @brief 使用密鑰字串進行加密或解密
 *
 * 整合密鑰衍生與加密操作的便利函式。
 *
 * @param key_str 密鑰字串（會自動擴展為 32 位元組）
 * @param nonce   12 位元組 nonce
 * @param input   輸入資料
 * @param output  輸出資料
 * @param len     資料長度
 */
void chacha20_encrypt_with_key(const char *key_str, const uint8_t *nonce, 
                                const uint8_t *input, uint8_t *output, size_t len);

/**
 * @brief 從密鑰字串衍生 32 位元組密鑰
 *
 * 將任意長度的密鑰字串擴展為標準的 32 位元組密鑰。
 *
 * @param key_str 密鑰字串
 * @param key     輸出的 32 位元組密鑰陣列
 *
 * @note 此為簡化實作，正式環境建議使用標準 KDF
 */
void chacha20_derive_key(const char *key_str, uint8_t *key);

#endif // CHACHA20_H
