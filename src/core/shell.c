/**
 * @file shell.c
 * @brief Shell 核心模組實作
 * 
 * 實作 Shell 的生命週期管理、命令分派和歷史記錄功能。
 * 具體的命令實作委託給 shell_commands 模組。
 */

#include "shell.h"
#include "shell_commands.h"
#include "shell_completion.h"
#include "../filesystem/vfs.h"
#include "../filesystem/vfs_persist.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include "../ui/splash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * 私有常數定義
 * ============================================================================ */

/** @brief 命令列最大長度 */
#define MAX_LINE_LEN 1024

/** @brief 命令參數最大數量 */
#define MAX_ARGS 64

/** @brief VFS 持久化檔案名稱 */
#define VFS_DATA_FILE ".yunfs_data"

/** @brief VFS 加密金鑰 */
#define ENCRYPTION_KEY "yunhongisbest"

/* ============================================================================
 * 命令表定義
 * ============================================================================ */

/**
 * @brief 命令處理函數類型
 */
typedef bool (*cmd_handler_t)(shell_t *shell, int argc, char **argv);

/**
 * @brief 命令表項目結構
 */
typedef struct {
    const char *name;       /**< 命令名稱 */
    cmd_handler_t handler;  /**< 命令處理函數 */
} cmd_entry_t;

/**
 * @brief 內建命令表
 * 
 * 使用表驅動方式管理命令，便於擴展和維護。
 */
static const cmd_entry_t cmd_table[] = {
    { "ls",      cmd_ls      },
    { "cd",      cmd_cd      },
    { "pwd",     cmd_pwd     },
    { "mkdir",   cmd_mkdir   },
    { "touch",   cmd_touch   },
    { "cat",     cmd_cat     },
    { "echo",    cmd_echo    },
    { "rm",      cmd_rm      },
    { "mv",      cmd_mv      },
    { "cp",      cmd_cp      },
    { "vim",     cmd_vim     },
    { "clear",   cmd_clear   },
    { "help",    cmd_help    },
    { "history", cmd_history },
    { "exit",    cmd_exit    },
    { NULL,      NULL        }  // 結束標記
};

/* ============================================================================
 * Shell 生命週期管理
 * ============================================================================ */

shell_t *shell_create(void) {
    shell_t *shell = (shell_t *)safe_malloc(sizeof(shell_t));
    if (shell == NULL) {
        return NULL;
    }
    
    // 嘗試從持久化檔案載入 VFS（加密格式）
    shell->vfs = vfs_load_encrypted(VFS_DATA_FILE, ENCRYPTION_KEY);
    if (shell->vfs == NULL) {
        // 載入失敗，創建新的空 VFS
        error_clear();
        shell->vfs = vfs_init();
        if (shell->vfs == NULL) {
            safe_free(shell);
            return NULL;
        }
    }
    
    // 初始化 Shell 狀態
    shell->current_dir = shell->vfs->root;
    shell->prompt = safe_strdup("yun-fs$ ");
    shell->running = true;
    shell->history_count = 0;
    
    // 初始化歷史記錄陣列
    for (int i = 0; i < HISTORY_MAX; i++) {
        shell->history[i] = NULL;
    }
    
    return shell;
}

void shell_destroy(shell_t *shell) {
    if (shell == NULL) {
        return;
    }
    
    // 保存 VFS 到持久化檔案
    if (shell->vfs != NULL) {
        vfs_save_encrypted(shell->vfs, VFS_DATA_FILE, ENCRYPTION_KEY);
        vfs_destroy(shell->vfs);
    }
    
    // 釋放歷史記錄
    for (int i = 0; i < shell->history_count; i++) {
        safe_free(shell->history[i]);
    }
    
    safe_free(shell->prompt);
    safe_free(shell);
}

/* ============================================================================
 * 命令解析與執行
 * ============================================================================ */

char **shell_parse_command(const char *line, int *argc) {
    if (line == NULL || argc == NULL) {
        return NULL;
    }
    
    char **args = (char **)safe_malloc(sizeof(char *) * MAX_ARGS);
    if (args == NULL) {
        return NULL;
    }
    
    *argc = 0;
    const char *p = line;
    
    // 跳過前導空白
    while (*p && isspace(*p)) p++;
    
    // 逐一提取以空白分隔的參數
    while (*p && *argc < MAX_ARGS - 1) {
        while (*p && isspace(*p)) p++;
        if (!*p) break;
        
        const char *start = p;
        while (*p && !isspace(*p)) p++;
        
        size_t len = p - start;
        if (len > 0) {
            args[*argc] = safe_strndup(start, len);
            if (args[*argc] == NULL) {
                // 記憶體分配失敗，清理已分配的參數
                for (int i = 0; i < *argc; i++) {
                    safe_free(args[i]);
                }
                safe_free(args);
                return NULL;
            }
            (*argc)++;
        }
    }
    
    args[*argc] = NULL;  // NULL 結尾
    return args;
}

void shell_free_args(char **args, int argc) {
    if (args == NULL) {
        return;
    }
    
    for (int i = 0; i < argc; i++) {
        safe_free(args[i]);
    }
    safe_free(args);
}

/**
 * @brief 在命令表中查找命令處理函數
 * @param name 命令名稱
 * @return 對應的處理函數，找不到返回 NULL
 */
static cmd_handler_t find_command_handler(const char *name) {
    for (int i = 0; cmd_table[i].name != NULL; i++) {
        if (strcmp(cmd_table[i].name, name) == 0) {
            return cmd_table[i].handler;
        }
    }
    return NULL;
}

bool shell_execute_command(shell_t *shell, const char *command) {
    if (shell == NULL || command == NULL) {
        return false;
    }
    
    // 跳過前導空白
    while (*command && isspace(*command)) command++;
    if (!*command) return true;  // 空命令視為成功
    
    // 解析命令
    int argc = 0;
    char **argv = shell_parse_command(command, &argc);
    if (argv == NULL || argc == 0) {
        return false;
    }
    
    // 查找並執行命令
    bool result = false;
    cmd_handler_t handler = find_command_handler(argv[0]);
    
    if (handler != NULL) {
        result = handler(shell, argc, argv);
    } else {
        printf("錯誤: 未知命令 '%s'。輸入 'help' 查看可用命令\n", argv[0]);
        result = false;
    }
    
    shell_free_args(argv, argc);
    return result;
}

/* ============================================================================
 * 歷史記錄管理
 * ============================================================================ */

void shell_add_history(shell_t *shell, const char *cmd) {
    if (shell == NULL || cmd == NULL || cmd[0] == '\0') {
        return;
    }
    
    // 跳過與上一條相同的命令（避免重複記錄）
    if (shell->history_count > 0 && 
        strcmp(shell->history[shell->history_count - 1], cmd) == 0) {
        return;
    }
    
    // 如果歷史記錄已滿，移除最舊的一條
    if (shell->history_count >= HISTORY_MAX) {
        safe_free(shell->history[0]);
        for (int i = 1; i < HISTORY_MAX; i++) {
            shell->history[i - 1] = shell->history[i];
        }
        shell->history_count = HISTORY_MAX - 1;
    }
    
    // 添加新記錄
    shell->history[shell->history_count] = safe_strdup(cmd);
    if (shell->history[shell->history_count] != NULL) {
        shell->history_count++;
    }
}

/* ============================================================================
 * Shell 主迴圈
 * ============================================================================ */

void shell_run(shell_t *shell) {
    if (shell == NULL) {
        return;
    }
    
    char line[MAX_LINE_LEN];
    
    // 顯示啟動畫面
    splash_show();
    
    // 主命令迴圈
    while (shell->running) {
        // 顯示提示符：綠色的當前路徑 + 提示符
        char *current_path = vfs_get_path(shell->current_dir);
        if (current_path != NULL) {
            printf("\033[32m%s\033[0m ", current_path);
            safe_free(current_path);
        }
        printf("%s", shell->prompt);
        fflush(stdout);
        
        // 讀取使用者輸入（支援 Tab 自動完成）
        if (!shell_read_line_with_completion(shell, line, sizeof(line))) {
            break;
        }
        
        // 記錄到歷史
        shell_add_history(shell, line);
        
        // 執行命令
        shell_execute_command(shell, line);
    }
}
