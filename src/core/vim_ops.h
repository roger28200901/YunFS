/**
 * @file vim_ops.h
 * @brief Vim 操作模組標頭檔
 * 
 * 本模組實作 Vim 編輯器的進階功能，包含：
 * - 撤銷/重做系統
 * - 暫存器（寄存器）操作
 * - Visual 模式
 * - 搜尋功能
 * - 文字物件操作
 * 
 * @note 設計考量：
 *   - 撤銷記錄使用雙向鏈結串列，支援有限數量的歷史
 *   - 暫存器支援 a-z 共 26 個具名暫存器
 *   - 搜尋支援向前/向後及循環搜尋
 */

#ifndef VIM_OPS_H
#define VIM_OPS_H

#include "buffer.h"
#include "../ui/input.h"
#include "../ui/screen.h"
#include <stdbool.h>

/* ============================================================================
 * 前向宣告
 * ============================================================================ */

/** @brief 編輯器結構（定義於 editor.h） */
struct editor;

/* ============================================================================
 * 列舉型別定義
 * ============================================================================ */

/**
 * @brief Vim 操作類型
 * 
 * 用於組合操作，如 d（刪除）+ w（單詞）= dw。
 */
typedef enum {
    VIM_OP_NONE,        /**< 無待處理操作 */
    VIM_OP_DELETE,      /**< 刪除操作（d） */
    VIM_OP_YANK,        /**< 複製操作（y） */
    VIM_OP_CHANGE,      /**< 修改操作（c） */
} vim_operation_t;

/**
 * @brief Visual 模式類型
 */
typedef enum {
    VISUAL_CHAR,        /**< 字元選取模式（v） */
    VISUAL_LINE,        /**< 行選取模式（V） */
    VISUAL_BLOCK,       /**< 區塊選取模式（Ctrl+V） */
} visual_mode_t;

/**
 * @brief 撤銷操作類型
 */
typedef enum {
    UNDO_INSERT_CHAR,   /**< 插入字元（撤銷時需刪除） */
    UNDO_DELETE_CHAR,   /**< 刪除字元（撤銷時需插入） */
    UNDO_INSERT_LINE,   /**< 插入行 */
    UNDO_DELETE_LINE,   /**< 刪除行 */
    UNDO_JOIN_LINE,     /**< 合併行 */
    UNDO_SPLIT_LINE,    /**< 分割行 */
} undo_type_t;

/* ============================================================================
 * 資料結構定義
 * ============================================================================ */

/**
 * @brief 撤銷記錄結構
 * 
 * 儲存單一操作的撤銷資訊，使用雙向鏈結串列連接。
 */
typedef struct undo_record {
    undo_type_t type;           /**< 操作類型 */
    size_t row;                 /**< 操作發生的行號 */
    size_t col;                 /**< 操作發生的欄位 */
    char *text;                 /**< 相關的文字內容 */
    size_t text_len;            /**< 文字長度 */
    struct undo_record *prev;   /**< 前一筆記錄 */
    struct undo_record *next;   /**< 後一筆記錄 */
} undo_record_t;

/**
 * @brief Vim 暫存器結構
 * 
 * 儲存複製/刪除的文字，供貼上使用。
 */
typedef struct vim_register {
    char *text;         /**< 暫存的文字內容 */
    size_t length;      /**< 文字長度 */
    bool is_line;       /**< 是否為整行模式（影響貼上行為） */
} vim_register_t;

/**
 * @brief Vim 操作上下文結構
 * 
 * 儲存 Vim 編輯器的所有狀態資訊。
 */
typedef struct vim_context {
    vim_operation_t op_type;        /**< 目前待處理的操作類型 */
    size_t count;                   /**< 重複次數（如 3dd） */
    bool pending;                   /**< 是否有待處理的操作符 */
    char last_key;                  /**< 最後按下的按鍵 */
    undo_record_t *undo_head;       /**< 撤銷記錄串列頭（最新） */
    undo_record_t *undo_tail;       /**< 撤銷記錄串列尾（最舊） */
    size_t undo_count;              /**< 目前撤銷記錄數量 */
    size_t undo_max;                /**< 最大撤銷記錄數量 */
    vim_register_t registers[26];   /**< a-z 具名暫存器 */
    vim_register_t default_register;/**< 預設暫存器（"） */
    visual_mode_t visual_mode;      /**< Visual 模式類型 */
    cursor_t visual_start;          /**< Visual 模式選取起點 */
    cursor_t visual_end;            /**< Visual 模式選取終點 */
    char *search_pattern;           /**< 目前的搜尋模式 */
    size_t search_direction;        /**< 搜尋方向（1=向前，-1=向後） */
} vim_context_t;

/* ============================================================================
 * 上下文管理
 * ============================================================================ */

/**
 * @brief 初始化 Vim 上下文
 * 
 * 將所有欄位設為初始值，配置必要的資源。
 * 
 * @param ctx 要初始化的上下文
 */
void vim_init_context(vim_context_t *ctx);

/**
 * @brief 清理 Vim 上下文
 * 
 * 釋放所有配置的資源，包含撤銷記錄和暫存器。
 * 
 * @param ctx 要清理的上下文
 */
void vim_cleanup_context(vim_context_t *ctx);

/* ============================================================================
 * 編輯器類型定義（避免循環引用）
 * ============================================================================ */

typedef struct editor editor_t;

/* ============================================================================
 * Normal 模式操作
 * ============================================================================ */

/**
 * @brief 處理 Normal 模式的移動按鍵
 * 
 * @param editor 編輯器實例
 * @param key 按鍵輸入
 * @param ctx Vim 上下文
 * @return 有處理按鍵回傳 true，否則回傳 false
 */
bool vim_normal_movement(editor_t *editor, key_input_t *key, vim_context_t *ctx);

/**
 * @brief 處理 Normal 模式的編輯按鍵
 * 
 * @param editor 編輯器實例
 * @param key 按鍵輸入
 * @param ctx Vim 上下文
 * @return 有處理按鍵回傳 true，否則回傳 false
 */
bool vim_normal_edit(editor_t *editor, key_input_t *key, vim_context_t *ctx);

/**
 * @brief 處理進入 Insert 模式的按鍵
 * 
 * @param editor 編輯器實例
 * @param key 按鍵輸入
 * @return 有處理按鍵回傳 true，否則回傳 false
 */
bool vim_normal_insert(editor_t *editor, key_input_t *key);

/* ============================================================================
 * Insert 模式操作
 * ============================================================================ */

/**
 * @brief 處理 Insert 模式的進階按鍵
 * 
 * @param editor 編輯器實例
 * @param key 按鍵輸入
 * @param ctx Vim 上下文
 * @return 有處理按鍵回傳 true，否則回傳 false
 */
bool vim_insert_advanced(editor_t *editor, key_input_t *key, vim_context_t *ctx);

/* ============================================================================
 * Visual 模式操作
 * ============================================================================ */

/**
 * @brief 處理 Visual 模式的按鍵
 * 
 * @param editor 編輯器實例
 * @param key 按鍵輸入
 * @param ctx Vim 上下文
 * @return 有處理按鍵回傳 true，否則回傳 false
 */
bool vim_visual_mode(editor_t *editor, key_input_t *key, vim_context_t *ctx);

/* ============================================================================
 * 撤銷/重做操作
 * ============================================================================ */

/**
 * @brief 執行撤銷操作
 * 
 * @param editor 編輯器實例
 * @param ctx Vim 上下文
 * @return 成功撤銷回傳 true，無可撤銷時回傳 false
 */
bool vim_undo(editor_t *editor, vim_context_t *ctx);

/**
 * @brief 執行重做操作
 * 
 * @param editor 編輯器實例
 * @param ctx Vim 上下文
 * @return 成功重做回傳 true，無可重做時回傳 false
 */
bool vim_redo(editor_t *editor, vim_context_t *ctx);

/**
 * @brief 記錄撤銷資訊
 * 
 * @param ctx Vim 上下文
 * @param type 操作類型
 * @param row 行號
 * @param col 欄位
 * @param text 相關文字
 * @param len 文字長度
 */
void vim_record_undo(vim_context_t *ctx, undo_type_t type, size_t row, size_t col, const char *text, size_t len);

/* ============================================================================
 * 暫存器操作
 * ============================================================================ */

/**
 * @brief 將文字複製到暫存器
 * 
 * @param ctx Vim 上下文
 * @param reg 暫存器名稱（a-z 或 A-Z，大寫為追加模式）
 * @param text 要儲存的文字
 * @param len 文字長度
 * @param is_line 是否為整行模式
 */
void vim_yank_to_register(vim_context_t *ctx, char reg, const char *text, size_t len, bool is_line);

/**
 * @brief 從暫存器取得文字
 * 
 * @param ctx Vim 上下文
 * @param reg 暫存器名稱
 * @param len 輸出參數，回傳文字長度
 * @param is_line 輸出參數，回傳是否為整行模式
 * @return 暫存器中的文字（不要釋放）
 */
const char *vim_get_register(vim_context_t *ctx, char reg, size_t *len, bool *is_line);

/* ============================================================================
 * 搜尋操作
 * ============================================================================ */

/**
 * @brief 向前搜尋文字
 * 
 * 從目前位置向檔案尾端搜尋，若到達尾端則從頭繼續。
 * 
 * @param editor 編輯器實例
 * @param pattern 搜尋模式
 * @return 找到回傳 true 並移動游標，否則回傳 false
 */
bool vim_search_forward(editor_t *editor, const char *pattern);

/**
 * @brief 向後搜尋文字
 * 
 * 從目前位置向檔案頭端搜尋，若到達頭端則從尾端繼續。
 * 
 * @param editor 編輯器實例
 * @param pattern 搜尋模式
 * @return 找到回傳 true 並移動游標，否則回傳 false
 */
bool vim_search_backward(editor_t *editor, const char *pattern);

/**
 * @brief 搜尋下一個匹配
 * 
 * 使用上次的搜尋模式，依原方向搜尋下一個匹配。
 * 
 * @param editor 編輯器實例
 * @param ctx Vim 上下文
 * @return 找到回傳 true，否則回傳 false
 */
bool vim_search_next(editor_t *editor, vim_context_t *ctx);

/**
 * @brief 搜尋上一個匹配
 * 
 * 使用上次的搜尋模式，依反方向搜尋上一個匹配。
 * 
 * @param editor 編輯器實例
 * @param ctx Vim 上下文
 * @return 找到回傳 true，否則回傳 false
 */
bool vim_search_prev(editor_t *editor, vim_context_t *ctx);

/* ============================================================================
 * 文字物件操作
 * ============================================================================ */

/**
 * @brief 找到單詞的開頭位置
 * 
 * @param buf 緩衝區
 * @param row 目前行號
 * @param col 目前欄位
 * @param new_col 輸出參數，回傳單詞開頭的欄位
 * @return 成功回傳 true，失敗回傳 false
 */
bool vim_find_word_start(buffer_t *buf, size_t row, size_t col, size_t *new_col);

/**
 * @brief 找到單詞的結尾位置
 * 
 * @param buf 緩衝區
 * @param row 目前行號
 * @param col 目前欄位
 * @param new_col 輸出參數，回傳單詞結尾的欄位
 * @return 成功回傳 true，失敗回傳 false
 */
bool vim_find_word_end(buffer_t *buf, size_t row, size_t col, size_t *new_col);

/**
 * @brief 向後找到前一個單詞
 * 
 * @param buf 緩衝區
 * @param row 目前行號
 * @param col 目前欄位
 * @param new_row 輸出參數，回傳新行號
 * @param new_col 輸出參數，回傳新欄位
 * @return 成功回傳 true，失敗回傳 false
 */
bool vim_find_word_backward(buffer_t *buf, size_t row, size_t col, size_t *new_row, size_t *new_col);

/**
 * @brief 找到行的第一個非空白字元位置
 * 
 * 對應 Vim 的 ^ 命令。
 * 
 * @param buf 緩衝區
 * @param row 行號
 * @param new_col 輸出參數，回傳位置
 * @return 成功回傳 true，失敗回傳 false
 */
bool vim_find_line_start(buffer_t *buf, size_t row, size_t *new_col);

/**
 * @brief 找到行尾位置
 * 
 * 對應 Vim 的 $ 命令。
 * 
 * @param buf 緩衝區
 * @param row 行號
 * @param new_col 輸出參數，回傳位置
 * @return 成功回傳 true，失敗回傳 false
 */
bool vim_find_line_end(buffer_t *buf, size_t row, size_t *new_col);

#endif // VIM_OPS_H
