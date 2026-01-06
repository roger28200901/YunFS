/**
 * @file input.h
 * @brief 鍵盤輸入處理模組標頭檔
 *
 * 本模組負責終端機的原始模式設定與鍵盤輸入處理，包含：
 * - 終端機原始模式（Raw Mode）的初始化與清理
 * - 按鍵讀取與解析（含特殊鍵、Escape 序列）
 * - 命令列輸入讀取
 *
 * @author Yun
 * @date 2025
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * 資料結構定義
 * ============================================================================ */

/**
 * @brief 鍵盤輸入結構
 *
 * 封裝單次按鍵的所有資訊，包含修飾鍵與 Escape 序列。
 */
typedef struct key_input {
    char key;              /**< 基本按鍵字元 */
    bool ctrl;             /**< Ctrl 鍵是否按下 */
    bool alt;              /**< Alt 鍵是否按下 */
    bool shift;            /**< Shift 鍵是否按下 */
    bool escape;           /**< 是否為 Escape 序列 */
    char escape_seq[16];   /**< Escape 序列內容 */
} key_input_t;

/* ============================================================================
 * 終端機初始化與清理
 * ============================================================================ */

/**
 * @brief 初始化輸入系統
 *
 * 將終端機設定為原始模式（Raw Mode），以便逐字元讀取輸入。
 *
 * @return true 成功，false 失敗
 *
 * @note 程式結束前必須呼叫 input_cleanup() 恢復終端機設定
 */
bool input_init(void);

/**
 * @brief 清理輸入系統
 *
 * 恢復終端機為正常模式（Canonical Mode）。
 */
void input_cleanup(void);

/* ============================================================================
 * 按鍵讀取函式
 * ============================================================================ */

/**
 * @brief 讀取單一按鍵輸入
 *
 * 阻塞式讀取，直到使用者按下按鍵。
 * 自動處理 Escape 序列（方向鍵、功能鍵等）。
 *
 * @param key 輸出參數，儲存按鍵資訊
 * @return true 成功讀取，false 讀取失敗或參數無效
 */
bool input_read_key(key_input_t *key);

/**
 * @brief 讀取一行輸入
 *
 * 用於命令模式，讀取直到使用者按下 Enter。
 * 支援 Backspace 刪除與 Ctrl+U 清除整行。
 *
 * @param buffer 輸出緩衝區
 * @param size   緩衝區大小
 * @return true 成功讀取（按下 Enter），false 取消（按下 Escape）
 */
bool input_read_line(char *buffer, size_t size);

/**
 * @brief 檢查是否為特殊按鍵
 *
 * 判斷按鍵是否包含修飾鍵（Ctrl/Alt）或為 Escape 序列。
 *
 * @param key 按鍵輸入結構
 * @return true 為特殊按鍵，false 為一般字元
 */
bool is_special_key(key_input_t *key);

#endif // INPUT_H
