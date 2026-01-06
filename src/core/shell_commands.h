/**
 * @file shell_commands.h
 * @brief Shell 命令處理模組
 * 
 * 定義所有 shell 內建命令的處理函數介面。
 * 每個命令遵循統一的函數簽名：bool cmd_xxx(shell_t *shell, int argc, char **argv)
 */

#ifndef SHELL_COMMANDS_H
#define SHELL_COMMANDS_H

#include "shell.h"
#include <stdbool.h>

/* ============================================================================
 * 檔案系統導航命令
 * ============================================================================ */

/**
 * @brief 列出目錄內容
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為可選的目錄路徑
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_ls(shell_t *shell, int argc, char **argv);

/**
 * @brief 切換當前工作目錄
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為目標目錄路徑
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_cd(shell_t *shell, int argc, char **argv);

/**
 * @brief 顯示當前工作目錄的完整路徑
 * @param shell Shell 實例
 * @param argc 參數數量（未使用）
 * @param argv 參數陣列（未使用）
 * @return 總是返回 true
 */
bool cmd_pwd(shell_t *shell, int argc, char **argv);

/* ============================================================================
 * 檔案/目錄操作命令
 * ============================================================================ */

/**
 * @brief 創建新目錄
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為目錄名稱
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_mkdir(shell_t *shell, int argc, char **argv);

/**
 * @brief 創建空檔案或更新檔案時間戳
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為檔案名稱
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_touch(shell_t *shell, int argc, char **argv);

/**
 * @brief 顯示檔案內容
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為檔案名稱
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_cat(shell_t *shell, int argc, char **argv);

/**
 * @brief 輸出文字到終端或重定向到檔案
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，支援 > 重定向語法
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_echo(shell_t *shell, int argc, char **argv);

/**
 * @brief 刪除檔案或目錄
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為 -r（可選）或目標路徑
 * @note 刪除目錄需要 -r 選項
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_rm(shell_t *shell, int argc, char **argv);

/**
 * @brief 移動或重命名檔案/目錄
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為源路徑，argv[2] 為目標路徑
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_mv(shell_t *shell, int argc, char **argv);

/**
 * @brief 複製檔案或目錄
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為源路徑，argv[2] 為目標路徑
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_cp(shell_t *shell, int argc, char **argv);

/* ============================================================================
 * 編輯器命令
 * ============================================================================ */

/**
 * @brief 使用內建 vim 編輯器開啟檔案
 * @param shell Shell 實例
 * @param argc 參數數量
 * @param argv 參數陣列，argv[1] 為檔案名稱
 * @return 成功返回 true，失敗返回 false
 */
bool cmd_vim(shell_t *shell, int argc, char **argv);

/* ============================================================================
 * Shell 控制命令
 * ============================================================================ */

/**
 * @brief 清除終端畫面
 * @param shell Shell 實例（未使用）
 * @param argc 參數數量（未使用）
 * @param argv 參數陣列（未使用）
 * @return 總是返回 true
 */
bool cmd_clear(shell_t *shell, int argc, char **argv);

/**
 * @brief 顯示可用命令的幫助資訊
 * @param shell Shell 實例（未使用）
 * @param argc 參數數量（未使用）
 * @param argv 參數陣列（未使用）
 * @return 總是返回 true
 */
bool cmd_help(shell_t *shell, int argc, char **argv);

/**
 * @brief 顯示命令歷史記錄
 * @param shell Shell 實例
 * @param argc 參數數量（未使用）
 * @param argv 參數陣列（未使用）
 * @return 總是返回 true
 */
bool cmd_history(shell_t *shell, int argc, char **argv);

/**
 * @brief 退出 shell
 * @param shell Shell 實例
 * @param argc 參數數量（未使用）
 * @param argv 參數陣列（未使用）
 * @return 總是返回 true
 */
bool cmd_exit(shell_t *shell, int argc, char **argv);

/* ============================================================================
 * 輔助函數
 * ============================================================================ */

/**
 * @brief 將相對路徑轉換為絕對路徑
 * @param shell Shell 實例
 * @param path 輸入路徑（相對或絕對）
 * @return 新分配的絕對路徑字串，呼叫者需負責釋放記憶體
 * @note 支援 "." 和 ".." 特殊路徑
 */
char *shell_get_full_path(shell_t *shell, const char *path);

#endif // SHELL_COMMANDS_H

