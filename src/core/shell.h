/**
 * @file shell.h
 * @brief Shell 核心模組
 * 
 * 定義 Shell 的核心資料結構和主要操作介面。
 * Shell 提供類似 Unix 的命令列介面，操作虛擬檔案系統。
 */

#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include "../filesystem/vfs.h"

/** @brief 最大歷史記錄數量 */
#define HISTORY_MAX 100

/**
 * @brief Shell 實例結構
 * 
 * 包含 Shell 運行所需的所有狀態資訊。
 */
typedef struct {
    vfs_t *vfs;                     /**< 虛擬文件系統實例 */
    vfs_node_t *current_dir;        /**< 當前工作目錄節點 */
    char *prompt;                   /**< 命令提示符字串 */
    bool running;                   /**< Shell 運行狀態旗標 */
    char *history[HISTORY_MAX];     /**< 命令歷史記錄陣列 */
    int history_count;              /**< 當前歷史記錄數量 */
} shell_t;

/* ============================================================================
 * Shell 生命週期管理
 * ============================================================================ */

/**
 * @brief 創建並初始化 Shell 實例
 * 
 * 嘗試從持久化檔案載入 VFS，若失敗則創建新的 VFS。
 * 初始化歷史記錄和其他狀態。
 * 
 * @return 新創建的 Shell 實例，失敗返回 NULL
 */
shell_t *shell_create(void);

/**
 * @brief 銷毀 Shell 實例並釋放資源
 * 
 * 將 VFS 保存到持久化檔案，釋放所有分配的記憶體。
 * 
 * @param shell 要銷毀的 Shell 實例
 */
void shell_destroy(shell_t *shell);

/**
 * @brief 運行 Shell 主迴圈
 * 
 * 進入互動式命令列迴圈，持續接受並執行使用者命令，
 * 直到使用者輸入 exit 或發生錯誤。
 * 
 * @param shell Shell 實例
 */
void shell_run(shell_t *shell);

/* ============================================================================
 * 命令處理
 * ============================================================================ */

/**
 * @brief 執行單一命令
 * 
 * 解析並執行給定的命令字串。
 * 
 * @param shell Shell 實例
 * @param command 要執行的命令字串
 * @return 命令執行成功返回 true，失敗返回 false
 */
bool shell_execute_command(shell_t *shell, const char *command);

/**
 * @brief 解析命令字串為參數陣列
 * 
 * 將空格分隔的命令字串解析為 argc/argv 格式。
 * 
 * @param line 命令字串
 * @param argc 輸出參數，返回參數數量
 * @return 參數字串陣列，呼叫者需使用 shell_free_args 釋放
 */
char **shell_parse_command(const char *line, int *argc);

/**
 * @brief 釋放命令參數陣列
 * @param args 由 shell_parse_command 返回的陣列
 * @param argc 參數數量
 */
void shell_free_args(char **args, int argc);

/* ============================================================================
 * 歷史記錄管理
 * ============================================================================ */

/**
 * @brief 添加命令到歷史記錄
 * 
 * 將命令加入歷史記錄，自動處理重複命令和記錄上限。
 * 
 * @param shell Shell 實例
 * @param cmd 要記錄的命令字串
 */
void shell_add_history(shell_t *shell, const char *cmd);

#endif // SHELL_H
