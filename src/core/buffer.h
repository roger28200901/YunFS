/**
 * @file buffer.h
 * @brief 文字緩衝區模組標頭檔
 * 
 * 本模組實作編輯器的核心資料結構 - 文字緩衝區。
 * 使用雙向鏈結串列儲存文字行，支援高效的插入與刪除操作。
 * 
 * @note 設計考量：
 *   - 採用鏈結串列而非陣列，以支援頻繁的行插入/刪除
 *   - 每行獨立管理記憶體，避免大區塊重新配置
 *   - 支援安全的記憶體清除（防止敏感資料殘留）
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * 資料結構定義
 * ============================================================================ */

/**
 * @brief 文字行結構
 * 
 * 代表緩衝區中的單一行文字，使用雙向鏈結串列連接。
 * 每行有獨立的容量管理，支援動態擴展。
 */
typedef struct line {
    char *text;              /**< 行內容（以 null 結尾的字串） */
    size_t length;           /**< 行長度（不包括結尾的 null 字元） */
    size_t capacity;         /**< 已配置的記憶體容量 */
    struct line *next;       /**< 指向下一行的指標 */
    struct line *prev;       /**< 指向上一行的指標 */
} line_t;

/**
 * @brief 文字緩衝區結構
 * 
 * 管理整個文件的內容，包含所有行的鏈結串列、
 * 檔案名稱、修改狀態等元資料。
 */
typedef struct {
    char *filename;          /**< 關聯的檔案名稱（可為 NULL） */
    line_t *head;            /**< 指向第一行的指標 */
    line_t *tail;            /**< 指向最後一行的指標 */
    size_t line_count;       /**< 總行數 */
    bool modified;           /**< 是否有未儲存的修改 */
    bool read_only;          /**< 是否為唯讀模式 */
} buffer_t;

/* ============================================================================
 * 緩衝區生命週期管理
 * ============================================================================ */

/**
 * @brief 建立新的空白緩衝區
 * 
 * 配置並初始化一個新的緩衝區，包含一個空行。
 * 
 * @param filename 關聯的檔案名稱（可為 NULL）
 * @return 新建立的緩衝區指標，失敗時回傳 NULL
 * 
 * @note 呼叫者需負責使用 buffer_destroy() 釋放記憶體
 */
buffer_t *buffer_create(const char *filename);

/**
 * @brief 銷毀緩衝區並釋放所有資源
 * 
 * 釋放緩衝區中所有行的記憶體，並清除敏感資料。
 * 
 * @param buf 要銷毀的緩衝區（可為 NULL，此時不做任何事）
 */
void buffer_destroy(buffer_t *buf);

/* ============================================================================
 * 檔案輸入/輸出操作
 * ============================================================================ */

/**
 * @brief 從檔案載入內容到緩衝區
 * 
 * 讀取指定檔案的內容，取代緩衝區現有的所有內容。
 * 載入後會清除修改標記。
 * 
 * @param buf 目標緩衝區
 * @param filename 要載入的檔案路徑
 * @return 成功回傳 true，失敗回傳 false 並設定錯誤訊息
 * 
 * @warning 此操作會清除緩衝區現有內容
 */
bool buffer_load_from_file(buffer_t *buf, const char *filename);

/**
 * @brief 將緩衝區內容儲存到檔案
 * 
 * 將所有行寫入指定檔案，每行以換行符號結尾。
 * 儲存成功後會清除修改標記。
 * 
 * @param buf 來源緩衝區
 * @param filename 目標檔案路徑（NULL 表示使用緩衝區原有檔名）
 * @return 成功回傳 true，失敗回傳 false 並設定錯誤訊息
 */
bool buffer_save_to_file(buffer_t *buf, const char *filename);

/* ============================================================================
 * 行操作
 * ============================================================================ */

/**
 * @brief 在指定位置插入新行
 * 
 * 在指定的行號位置插入一行新文字。原本在該位置及之後的行會向下移動。
 * 
 * @param buf 目標緩衝區
 * @param line_num 插入位置（0 表示最前面）
 * @param text 新行的文字內容
 * @return 成功回傳 true，失敗回傳 false
 * 
 * @note 若 line_num 超過總行數，則插入到最後
 */
bool buffer_insert_line(buffer_t *buf, size_t line_num, const char *text);

/**
 * @brief 刪除指定行
 * 
 * 移除指定行號的行。若緩衝區只剩一行，則清空該行內容而非刪除。
 * 
 * @param buf 目標緩衝區
 * @param line_num 要刪除的行號（從 0 開始）
 * @return 成功回傳 true，失敗回傳 false
 * 
 * @note 緩衝區至少會保留一行（空行）
 */
bool buffer_delete_line(buffer_t *buf, size_t line_num);

/**
 * @brief 取得指定行的指標
 * 
 * @param buf 來源緩衝區
 * @param line_num 行號（從 0 開始）
 * @return 該行的指標，若行號無效則回傳最後一行或 NULL
 */
line_t *buffer_get_line(buffer_t *buf, size_t line_num);

/* ============================================================================
 * 字元操作
 * ============================================================================ */

/**
 * @brief 在指定位置插入字元
 * 
 * 在指定行的指定欄位插入單一字元。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號（從 0 開始）
 * @param col 欄位（從 0 開始）
 * @param c 要插入的字元
 * @return 成功回傳 true，失敗回傳 false
 * 
 * @note 若欄位超過行長度，則插入到行尾
 */
bool buffer_insert_char(buffer_t *buf, size_t line_num, size_t col, char c);

/**
 * @brief 刪除指定位置的字元
 * 
 * 刪除指定行的指定欄位的字元。
 * 
 * @param buf 目標緩衝區
 * @param line_num 行號（從 0 開始）
 * @param col 欄位（從 0 開始）
 * @return 成功回傳 true，失敗回傳 false
 */
bool buffer_delete_char(buffer_t *buf, size_t line_num, size_t col);

/* ============================================================================
 * 狀態查詢與設定
 * ============================================================================ */

/**
 * @brief 取得緩衝區總行數
 * @param buf 來源緩衝區
 * @return 總行數，若 buf 為 NULL 則回傳 0
 */
size_t buffer_get_line_count(buffer_t *buf);

/**
 * @brief 標記緩衝區為已修改狀態
 * @param buf 目標緩衝區
 */
void buffer_mark_modified(buffer_t *buf);

/**
 * @brief 清除緩衝區的修改標記
 * @param buf 目標緩衝區
 */
void buffer_clear_modified(buffer_t *buf);

/**
 * @brief 檢查緩衝區是否有未儲存的修改
 * @param buf 來源緩衝區
 * @return 有修改回傳 true，否則回傳 false
 */
bool buffer_is_modified(buffer_t *buf);

#endif // BUFFER_H
