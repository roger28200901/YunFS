#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "src/core/shell.h"
#include "src/core/editor.h"
#include "src/utils/error.h"

static shell_t *g_shell = NULL;

/**
 * @brief 信號處理函式，確保 Ctrl+C 時也能儲存 VFS
 */
static void signal_handler(int sig) {
    (void)sig;
    if (g_shell != NULL) {
        printf("\n收到中斷信號，正在儲存...\n");
        shell_destroy(g_shell);
        g_shell = NULL;
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    // 檢查是否要啟動編輯器
    if (argc > 1) {
        const char *filename = argv[1];
        
        // 檢查是否為幫助命令
        if (strcmp(filename, "--help") == 0 || strcmp(filename, "-h") == 0) {
            printf("Yun File System\n");
            printf("用法:\n");
            printf("  %s              - 啟動文件系統 shell\n", argv[0]);
            printf("  %s <檔案名稱>   - 使用編輯器打開檔案\n", argv[0]);
            printf("\nShell 命令:\n");
            printf("  ls, cd, pwd, mkdir, touch, cat, echo, rm, mv, cp, clear, help, exit\n");
            printf("\n編輯器命令:\n");
            printf("  :w, :q, :wq, :q!, :e <檔名>, :b <n>\n");
            return 0;
        }
        
        // 啟動編輯器
        editor_t *editor = editor_create();
        if (editor == NULL) {
            fprintf(stderr, "錯誤: 無法創建編輯器\n");
            return 1;
        }
        
        if (editor_open_file(editor, filename)) {
            editor_run(editor);
        } else {
            error_clear();
            if (editor_open_file(editor, filename)) {
                editor_run(editor);
            }
        }
        
        editor_destroy(editor);
    } else {
        // 啟動 Shell
        g_shell = shell_create();
        if (g_shell == NULL) {
            fprintf(stderr, "錯誤: 無法創建 shell\n");
            return 1;
        }
        
        // 註冊信號處理，確保 Ctrl+C 時儲存 VFS
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        shell_run(g_shell);
        shell_destroy(g_shell);
        g_shell = NULL;
    }
    
    return 0;
}
