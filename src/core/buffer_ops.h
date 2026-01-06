/**
 * @file buffer_ops.h
 * @brief 緩衝區進階操作模組標頭檔
 * 
 * 本模組提供 buffer.h 的擴充功能，包含更複雜的文字操作，
 * 如行合併、分割、單詞刪除、文字複製等 Vim 風格的編輯操作。
 * 
 * @note 設計考量：
 *   - 與 buffer.h 分離，保持基礎模組的簡潔
 *   - 提供 Vim 編輯器所需的進階操作
 *   - 所有操作都會正確設定 modified 旗標
 */

#ifndef BUFFER_OPS_H
#define BUFFER_OPS_H

#include "buffer.h"
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * 字元操作
 * ============================================================================ */

/**
 * @brief 替換指定位置的字元
 * 
 * 將指定行、列位置的字元替換為新字元（Vim 的 r 命令）。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號（從 0 開始）
 * @param col 欄位（從 0 開始）
 * @param c 新字元
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_replace_char(buffer_t *buf, size_t line_num, size_t col, char c);

/* ============================================================================
 * 行操作
 * ============================================================================ */

/**
 * @brief 合併兩行
 * 
 * 將指定行與下一行合併（Vim 的 J 命令）。
 * 
 * @param buf 目標緩衝區
 * @param line_num 要合併的第一行行號
 * @return 成功回傳 true，失敗回傳 false
 * 
 * @note 若 line_num 是最後一行，則無法合併
 */
bool buffer_join_lines(buffer_t *buf, size_t line_num);

/**
 * @brief 在指定位置分割行
 * 
 * 將一行從指定欄位分割成兩行。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號
 * @param col 分割位置
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_split_line(buffer_t *buf, size_t line_num, size_t col);

/* ============================================================================
 * 區段刪除操作
 * ============================================================================ */

/**
 * @brief 刪除從指定位置到行尾的文字
 * 
 * 對應 Vim 的 D 或 d$ 命令。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號
 * @param col 起始欄位
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_delete_to_end(buffer_t *buf, size_t line_num, size_t col);

/**
 * @brief 刪除從行首到指定位置的文字
 * 
 * 對應 Vim 的 d0 或 d^ 命令。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號
 * @param col 結束欄位（不包含）
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_delete_to_start(buffer_t *buf, size_t line_num, size_t col);

/**
 * @brief 刪除從目前位置到單詞結尾的文字
 * 
 * 對應 Vim 的 dw 命令。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號
 * @param col 起始欄位
 * @param new_col 輸出參數，回傳刪除後的游標位置
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_delete_word(buffer_t *buf, size_t line_num, size_t col, size_t *new_col);

/**
 * @brief 刪除從單詞開頭到目前位置的文字
 * 
 * 對應 Vim 的 db 命令。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號
 * @param col 結束欄位
 * @param new_col 輸出參數，回傳刪除後的游標位置
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_delete_word_backward(buffer_t *buf, size_t line_num, size_t col, size_t *new_col);

/* ============================================================================
 * 文字複製操作
 * ============================================================================ */

/**
 * @brief 複製整行文字
 * 
 * 對應 Vim 的 yy 命令。
 * 
 * @param buf 來源緩衝區
 * @param line_num 行號
 * @param len 輸出參數，回傳複製的長度
 * @return 新配置的字串，呼叫者需負責釋放
 */
char *buffer_copy_line(buffer_t *buf, size_t line_num, size_t *len);

/**
 * @brief 複製從指定位置到行尾的文字
 * 
 * 對應 Vim 的 y$ 命令。
 * 
 * @param buf 來源緩衝區
 * @param line_num 行號
 * @param col 起始欄位
 * @param len 輸出參數，回傳複製的長度
 * @return 新配置的字串，呼叫者需負責釋放
 */
char *buffer_copy_to_end(buffer_t *buf, size_t line_num, size_t col, size_t *len);

/**
 * @brief 複製從行首到指定位置的文字
 * 
 * 對應 Vim 的 y0 命令。
 * 
 * @param buf 來源緩衝區
 * @param line_num 行號
 * @param col 結束欄位（不包含）
 * @param len 輸出參數，回傳複製的長度
 * @return 新配置的字串，呼叫者需負責釋放
 */
char *buffer_copy_to_start(buffer_t *buf, size_t line_num, size_t col, size_t *len);

/**
 * @brief 複製目前位置的單詞
 * 
 * 對應 Vim 的 yw 命令。
 * 
 * @param buf 來源緩衝區
 * @param line_num 行號
 * @param col 起始欄位
 * @param len 輸出參數，回傳複製的長度
 * @return 新配置的字串，呼叫者需負責釋放
 */
char *buffer_copy_word(buffer_t *buf, size_t line_num, size_t col, size_t *len);

/* ============================================================================
 * 文字插入與替換
 * ============================================================================ */

/**
 * @brief 在指定位置插入文字字串
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號
 * @param col 插入位置
 * @param text 要插入的文字
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_insert_text(buffer_t *buf, size_t line_num, size_t col, const char *text);

/**
 * @brief 替換指定範圍的文字
 * 
 * 將 col_start 到 col_end 之間的文字替換為新文字。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號
 * @param col_start 起始欄位
 * @param col_end 結束欄位
 * @param text 替換的新文字
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_replace_text(buffer_t *buf, size_t line_num, size_t col_start, size_t col_end, const char *text);

/* ============================================================================
 * 文字取得（與複製相同，為相容性保留）
 * ============================================================================ */

/**
 * @brief 取得從指定位置到行尾的文字
 * @see buffer_copy_to_end
 */
char *buffer_get_to_end(buffer_t *buf, size_t line_num, size_t col, size_t *len);

/**
 * @brief 取得從行首到指定位置的文字
 * @see buffer_copy_to_start
 */
char *buffer_get_to_start(buffer_t *buf, size_t line_num, size_t col, size_t *len);

/* ============================================================================
 * 字元分類輔助函式
 * ============================================================================ */

/**
 * @brief 檢查字元是否為單詞組成字元
 * 
 * 單詞字元包含：字母、數字、底線。
 * 
 * @param c 要檢查的字元
 * @return 是單詞字元回傳 true，否則回傳 false
 */
bool is_word_char(char c);

/**
 * @brief 檢查字元是否為空白字元
 * 
 * 空白字元包含：空格、Tab。
 * 
 * @param c 要檢查的字元
 * @return 是空白字元回傳 true，否則回傳 false
 */
bool is_whitespace(char c);

#endif // BUFFER_OPS_H
