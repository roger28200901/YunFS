/**
 * @file editor.h
 * @brief 編輯器核心模組標頭檔
 * 
 * 本模組實作類 Vim 的文字編輯器，支援多緩衝區、
 * 多種編輯模式（Normal/Insert/Visual/Command）。
 * 
 * @note 設計考量：
 *   - 支援多個檔案同時編輯（多緩衝區）
 *   - 模式化編輯介面（仿 Vim）
 *   - 與 UI 模組解耦，便於測試與維護
 * 
 * @author Yun
 * @date 2025
 */

#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#include <stddef.h>
#include "buffer.h"

/* ============================================================================
 * 編輯器模式定義（核心概念，應定義於此）
 * ============================================================================ */

/**
 * @brief 編輯器模式列舉
 * 
 * 定義編輯器支援的四種操作模式，仿照 Vim 設計。
 */
typedef enum {
    MODE_NORMAL,      /**< 正常模式：瀏覽與指令 */
    MODE_INSERT,      /**< 插入模式：文字輸入 */
    MODE_VISUAL,      /**< 可視模式：區塊選取 */
    MODE_COMMAND      /**< 命令模式：執行 Ex 命令 */
} editor_mode_t;

/* ============================================================================
 * 前向宣告
 * ============================================================================ */

/**
 * @brief 鍵盤輸入結構（定義於 ui/input.h）
 */
struct key_input;
typedef struct key_input key_input_t;

/**
 * @brief 游標位置結構（定義於 ui/screen.h）
 */
struct cursor;
typedef struct cursor cursor_t;

/**
 * @brief Vim 操作上下文（定義於 vim_ops.h）
 * 
 * 儲存 Vim 特有的操作狀態，如暫存器、搜尋模式等。
 */
struct vim_context;
typedef struct vim_context vim_context_t;

/* ============================================================================
 * 資料結構定義
 * ============================================================================ */

/**
 * @brief 編輯器主結構
 * 
 * 管理編輯器的所有狀態，包含緩衝區、游標、模式等。
 */
typedef struct editor {
    buffer_t **buffers;        /**< 緩衝區指標陣列 */
    size_t buffer_count;       /**< 目前開啟的緩衝區數量 */
    size_t current_buffer;     /**< 目前作用中的緩衝區索引 */
    size_t cursor_row;         /**< 游標行位置 */
    size_t cursor_col;         /**< 游標列位置 */
    editor_mode_t mode;        /**< 目前的編輯模式 */
    size_t first_line;         /**< 畫面顯示的起始行號（用於捲動） */
    char *command_buffer;      /**< 命令模式的輸入緩衝區 */
    bool running;              /**< 編輯器是否正在執行 */
    vim_context_t *vim_ctx;    /**< Vim 操作上下文 */
    size_t repeat_count;       /**< 重複次數（如 3dd 中的 3） */
    char last_operation;       /**< 最後執行的操作（用於 . 命令） */
} editor_t;

/* ============================================================================
 * 編輯器生命週期管理
 * ============================================================================ */

/**
 * @brief 建立新的編輯器實例
 * 
 * 配置並初始化編輯器所需的所有資源。
 * 
 * @return 新建立的編輯器指標，失敗時回傳 NULL
 * @note 呼叫者需負責使用 editor_destroy() 釋放資源
 */
editor_t *editor_create(void);

/**
 * @brief 銷毀編輯器並釋放所有資源
 * 
 * 關閉所有緩衝區、釋放 Vim 上下文等資源。
 * 
 * @param editor 要銷毀的編輯器（可為 NULL）
 */
void editor_destroy(editor_t *editor);

/* ============================================================================
 * 緩衝區管理
 * ============================================================================ */

/**
 * @brief 開啟檔案到新緩衝區
 * 
 * 若檔案已開啟，則切換到該緩衝區；
 * 若檔案不存在，則建立新的空緩衝區。
 * 
 * @param editor 編輯器實例
 * @param filename 要開啟的檔案路徑
 * @return 成功回傳 true，失敗回傳 false
 */
bool editor_open_file(editor_t *editor, const char *filename);

/**
 * @brief 關閉目前的緩衝區
 * 
 * 若有未儲存的修改，目前會直接關閉（應改為提示使用者）。
 * 若關閉最後一個緩衝區，編輯器會結束執行。
 * 
 * @param editor 編輯器實例
 * @return 成功回傳 true，失敗回傳 false
 */
bool editor_close_buffer(editor_t *editor);

/**
 * @brief 切換到指定的緩衝區
 * 
 * @param editor 編輯器實例
 * @param index 目標緩衝區的索引（從 0 開始）
 * @return 成功回傳 true，索引無效時回傳 false
 */
bool editor_switch_buffer(editor_t *editor, size_t index);

/* ============================================================================
 * 輸入處理與主迴圈
 * ============================================================================ */

/**
 * @brief 處理鍵盤輸入
 * 
 * 根據目前的編輯模式處理按鍵，執行對應的操作。
 * 
 * @param editor 編輯器實例
 * @param key 按鍵輸入結構
 */
void editor_handle_input(editor_t *editor, key_input_t *key);

/**
 * @brief 執行編輯器主迴圈
 * 
 * 初始化終端機、進入互動式編輯迴圈，
 * 直到使用者退出或發生錯誤。
 * 
 * @param editor 編輯器實例
 */
void editor_run(editor_t *editor);

/* ============================================================================
 * 檔案儲存
 * ============================================================================ */

/**
 * @brief 儲存目前緩衝區到原檔案
 * 
 * @param editor 編輯器實例
 * @return 成功回傳 true，失敗回傳 false
 */
bool editor_save(editor_t *editor);

/**
 * @brief 另存目前緩衝區到指定檔案
 * 
 * @param editor 編輯器實例
 * @param filename 目標檔案路徑
 * @return 成功回傳 true，失敗回傳 false
 */
bool editor_save_as(editor_t *editor, const char *filename);

/* ============================================================================
 * 模式管理
 * ============================================================================ */

/**
 * @brief 切換編輯模式
 * 
 * @param editor 編輯器實例
 * @param mode 目標模式（Normal/Insert/Visual/Command）
 */
void editor_set_mode(editor_t *editor, editor_mode_t mode);

/**
 * @brief 取得目前的編輯模式
 * 
 * @param editor 編輯器實例
 * @return 目前的編輯模式，若 editor 為 NULL 則回傳 MODE_NORMAL
 */
editor_mode_t editor_get_mode(editor_t *editor);

#endif // EDITOR_H
