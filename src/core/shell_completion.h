/**
 * @file shell_completion.h
 * @brief Shell Tab 自動完成模組
 * 
 * 提供命令列輸入時的 Tab 鍵自動完成功能，
 * 包含檔案名稱補全和輸入處理。
 */

#ifndef SHELL_COMPLETION_H
#define SHELL_COMPLETION_H

#include "shell.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 支援 Tab 自動完成的行輸入函數
 * 
 * 提供完整的命令列編輯功能：
 * - Tab: 自動完成檔案/目錄名稱
 * - 左右方向鍵: 移動游標
 * - Backspace: 刪除前一個字元
 * - Ctrl+A: 移到行首
 * - Ctrl+E: 移到行尾
 * - Ctrl+U: 清除整行
 * 
 * @param shell Shell 實例，用於存取 VFS 進行補全
 * @param buffer 輸出緩衝區
 * @param size 緩衝區大小
 * @return 成功讀取返回 true，EOF 或錯誤返回 false
 */
bool shell_read_line_with_completion(shell_t *shell, char *buffer, size_t size);

/**
 * @brief 獲取符合前綴的檔案/目錄名稱列表
 * 
 * 根據輸入的前綴字串，在當前目錄或指定目錄中搜尋
 * 所有符合的檔案和目錄名稱。
 * 
 * @param shell Shell 實例
 * @param prefix 要匹配的前綴字串
 * @param count 輸出參數，返回匹配項目的數量
 * @return 匹配的字串陣列，呼叫者需使用 free_completions 釋放
 * @note 目錄名稱會自動加上 "/" 後綴
 */
char **shell_get_completions(shell_t *shell, const char *prefix, size_t *count);

/**
 * @brief 釋放補全結果陣列
 * @param completions 由 shell_get_completions 返回的陣列
 * @param count 陣列中的項目數量
 */
void shell_free_completions(char **completions, size_t count);

/**
 * @brief 找出所有補全項目的共同前綴
 * 
 * 當有多個匹配項目時，找出它們的最長共同前綴，
 * 用於部分補全。
 * 
 * @param completions 補全項目陣列
 * @param count 項目數量
 * @return 新分配的共同前綴字串，呼叫者需負責釋放
 */
char *shell_find_common_prefix(char **completions, size_t count);

#endif // SHELL_COMPLETION_H

