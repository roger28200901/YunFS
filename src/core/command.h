/**
 * @file command.h
 * @brief 命令解析模組標頭檔
 * 
 * 本模組負責解析 Vim 風格的命令列指令，
 * 如 :w、:q、:wq、:e <file> 等。
 * 
 * @note 設計考量：
 *   - 命令結構與執行邏輯分離
 *   - 支援帶參數的命令
 *   - 支援強制執行標記（如 :q!）
 */

#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>
#include "../ui/input.h"

/* ============================================================================
 * 列舉型別定義
 * ============================================================================ */

/**
 * @brief 命令類型列舉
 * 
 * 定義所有支援的 Vim 風格命令。
 */
typedef enum {
    CMD_QUIT,           /**< :q - 退出（有未儲存修改時會失敗） */
    CMD_QUIT_FORCE,     /**< :q! - 強制退出（捨棄未儲存的修改） */
    CMD_WRITE,          /**< :w [file] - 儲存（可選擇另存新檔） */
    CMD_WRITE_QUIT,     /**< :wq - 儲存並退出 */
    CMD_EDIT,           /**< :e <file> - 開啟檔案 */
    CMD_BUFFER,         /**< :b <n> - 切換到第 n 個緩衝區 */
    CMD_SUBSTITUTE,     /**< :s/old/new/ - 搜尋並替換 */
    CMD_SEARCH,         /**< /pattern - 搜尋文字 */
    CMD_SET,            /**< :set <option> - 設定選項 */
    CMD_UNKNOWN         /**< 無法識別的命令 */
} command_type_t;

/* ============================================================================
 * 資料結構定義
 * ============================================================================ */

/**
 * @brief 已解析的命令結構
 * 
 * 儲存命令類型及其參數，供執行函式使用。
 */
typedef struct {
    command_type_t type;    /**< 命令類型 */
    char *arg1;             /**< 第一個參數（如檔案名稱、搜尋模式） */
    char *arg2;             /**< 第二個參數（如替換字串） */
    bool force;             /**< 強制執行標記（對應 ! 修飾符） */
} command_t;

/* ============================================================================
 * 函式宣告
 * ============================================================================ */

/**
 * @brief 解析命令字串
 * 
 * 將使用者輸入的命令字串解析為結構化的命令物件。
 * 
 * @param cmd_str 命令字串（不含前導的 ':'）
 * @return 新配置的命令結構，失敗時回傳 NULL
 * 
 * @note 呼叫者需負責使用 command_free() 釋放記憶體
 * 
 * @par 支援的命令格式：
 *   - q, q! - 退出
 *   - w [filename] - 儲存
 *   - wq - 儲存並退出
 *   - e <filename> - 開啟檔案
 *   - b <number> - 切換緩衝區
 *   - s/old/new/ - 搜尋替換
 *   - set <option> - 設定選項
 */
command_t *command_parse(const char *cmd_str);

/**
 * @brief 釋放命令結構的記憶體
 * 
 * @param cmd 要釋放的命令結構（可為 NULL）
 */
void command_free(command_t *cmd);

/**
 * @brief 執行命令
 * 
 * @param cmd 要執行的命令
 * @param context 執行上下文（通常為編輯器實例）
 * @return 執行成功回傳 true，失敗回傳 false
 * 
 * @note 此函式目前為預留介面，實際執行邏輯在編輯器中實作
 */
bool command_execute(command_t *cmd, void *context);

#endif // COMMAND_H
