/**
 * @file shell_commands.c
 * @brief Shell 命令處理模組實作
 * 
 * 實作所有 shell 內建命令的處理邏輯。
 */

#include "shell_commands.h"
#include "shell.h"
#include "editor.h"
#include "../filesystem/vfs.h"
#include "../filesystem/path.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 私有輔助函數
 * ============================================================================ */

/**
 * @brief 遞迴複製 VFS 節點
 * @param vfs VFS 實例
 * @param src 源節點
 * @param dst_path 目標路徑
 * @return 成功返回 true，失敗返回 false
 */
static bool copy_node_recursive(vfs_t *vfs, vfs_node_t *src, const char *dst_path) {
    if (src == NULL || dst_path == NULL) {
        return false;
    }
    
    if (src->type == VFS_FILE) {
        // 複製檔案：讀取源檔案內容並創建新檔案
        size_t size = 0;
        void *data = vfs_read_file(src, &size);
        if (data != NULL) {
            vfs_node_t *new_file = vfs_create_file(vfs, dst_path, data, size);
            safe_free(data);
            return new_file != NULL;
        }
        return false;
    } else {
        // 複製目錄：先創建目錄，再遞迴複製所有子節點
        vfs_node_t *new_dir = vfs_create_dir(vfs, dst_path);
        if (new_dir == NULL) {
            return false;
        }
        
        vfs_node_t *child = src->children;
        while (child != NULL) {
            // 構建子節點的目標路徑
            char *child_dst_path = (char *)safe_malloc(strlen(dst_path) + strlen(child->name) + 2);
            if (child_dst_path != NULL) {
                strcpy(child_dst_path, dst_path);
                if (dst_path[strlen(dst_path) - 1] != '/') {
                    strcat(child_dst_path, "/");
                }
                strcat(child_dst_path, child->name);
                
                if (!copy_node_recursive(vfs, child, child_dst_path)) {
                    safe_free(child_dst_path);
                    return false;
                }
                safe_free(child_dst_path);
            }
            child = child->next;
        }
        
        return true;
    }
}

/* ============================================================================
 * 公開輔助函數
 * ============================================================================ */

char *shell_get_full_path(shell_t *shell, const char *path) {
    if (path == NULL) {
        return NULL;
    }
    
    // 處理特殊路徑 "."（當前目錄）
    if (strcmp(path, ".") == 0) {
        return vfs_get_path(shell->current_dir);
    }
    
    // 處理特殊路徑 ".."（父目錄）
    if (strcmp(path, "..") == 0) {
        if (shell->current_dir->parent != NULL) {
            return vfs_get_path(shell->current_dir->parent);
        } else {
            return safe_strdup("/");
        }
    }
    
    // 絕對路徑直接返回副本
    if (path[0] == '/') {
        return safe_strdup(path);
    }
    
    // 相對路徑：與當前目錄路徑連接
    char *current_path = vfs_get_path(shell->current_dir);
    if (current_path == NULL) {
        return NULL;
    }
    
    size_t current_len = strlen(current_path);
    size_t path_len = strlen(path);
    char *full_path = (char *)safe_malloc(current_len + path_len + 2);
    if (full_path == NULL) {
        safe_free(current_path);
        return NULL;
    }
    
    strncpy(full_path, current_path, current_len);
    // 確保路徑分隔符
    if (current_path[current_len - 1] != '/') {
        full_path[current_len] = '/';
        current_len++;
    }
    strncpy(full_path + current_len, path, path_len);
    full_path[current_len + path_len] = '\0';
    
    safe_free(current_path);
    return full_path;
}

/* ============================================================================
 * 檔案系統導航命令實作
 * ============================================================================ */

bool cmd_ls(shell_t *shell, int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : ".";
    
    // 取得目標目錄節點
    vfs_node_t *dir;
    if (strcmp(path, ".") == 0) {
        dir = shell->current_dir;
    } else {
        char *full_path = shell_get_full_path(shell, path);
        if (full_path == NULL) {
            printf("錯誤: 無法解析路徑\n");
            return false;
        }
        
        dir = vfs_find_node(shell->vfs, full_path);
        safe_free(full_path);
        
        if (dir == NULL || dir->type != VFS_DIR) {
            printf("錯誤: 目錄不存在\n");
            return false;
        }
    }
    
    // 列出目錄內容
    size_t count = 0;
    vfs_node_t **children = vfs_list_dir(dir, &count);
    
    if (children == NULL || count == 0) {
        printf("(空目錄)\n");
        return true;
    }
    
    // 輸出每個子節點，目錄以藍色顯示
    for (size_t i = 0; i < count; i++) {
        if (children[i]->type == VFS_DIR) {
            printf("\033[34m%s\033[0m/\n", children[i]->name);
        } else {
            printf("%s\n", children[i]->name);
        }
    }
    
    safe_free(children);
    return true;
}

bool cmd_cd(shell_t *shell, int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "/";
    
    char *full_path = shell_get_full_path(shell, path);
    if (full_path == NULL) {
        printf("錯誤: 無法解析路徑\n");
        return false;
    }
    
    vfs_node_t *dir = vfs_find_node(shell->vfs, full_path);
    safe_free(full_path);
    
    if (dir == NULL || dir->type != VFS_DIR) {
        printf("錯誤: 目錄不存在\n");
        return false;
    }
    
    shell->current_dir = dir;
    return true;
}

bool cmd_pwd(shell_t *shell, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    char *path = vfs_get_path(shell->current_dir);
    if (path != NULL) {
        printf("%s\n", path);
        safe_free(path);
    }
    return true;
}

/* ============================================================================
 * 檔案/目錄操作命令實作
 * ============================================================================ */

bool cmd_mkdir(shell_t *shell, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: mkdir <目錄名稱>\n");
        return false;
    }
    
    char *full_path = shell_get_full_path(shell, argv[1]);
    if (full_path == NULL) {
        printf("錯誤: 無法解析路徑\n");
        return false;
    }
    
    vfs_node_t *dir = vfs_create_dir(shell->vfs, full_path);
    if (dir == NULL) {
        error_t err = error_get();
        if (err.code != ERR_OK) {
            printf("錯誤: %s\n", err.message);
            error_clear();
        } else {
            printf("錯誤: 無法創建目錄\n");
        }
        safe_free(full_path);
        return false;
    }
    
    safe_free(full_path);
    return true;
}

bool cmd_touch(shell_t *shell, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: touch <檔案名稱>\n");
        return false;
    }
    
    char *full_path = shell_get_full_path(shell, argv[1]);
    if (full_path == NULL) {
        printf("錯誤: 無法解析路徑\n");
        return false;
    }
    
    vfs_node_t *file = vfs_create_file(shell->vfs, full_path, NULL, 0);
    safe_free(full_path);
    
    if (file == NULL) {
        error_t err = error_get();
        if (err.code != ERR_OK) {
            printf("錯誤: %s\n", err.message);
            error_clear();
        } else {
            printf("錯誤: 無法創建檔案\n");
        }
        return false;
    }
    
    return true;
}

bool cmd_cat(shell_t *shell, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: cat <檔案名稱>\n");
        return false;
    }
    
    char *full_path = shell_get_full_path(shell, argv[1]);
    if (full_path == NULL) {
        printf("錯誤: 無法解析路徑\n");
        return false;
    }
    
    vfs_node_t *file = vfs_find_node(shell->vfs, full_path);
    safe_free(full_path);
    
    if (file == NULL || file->type != VFS_FILE) {
        printf("錯誤: 檔案不存在\n");
        return false;
    }
    
    // 讀取並輸出檔案內容
    size_t size = 0;
    void *data = vfs_read_file(file, &size);
    if (data != NULL && size > 0) {
        fwrite(data, 1, size, stdout);
        printf("\n");
        safe_free(data);
    }
    
    return true;
}

bool cmd_echo(shell_t *shell, int argc, char **argv) {
    if (argc < 2) {
        printf("\n");
        return true;
    }
    
    // 檢查是否有重定向符號 ">"
    int redirect_idx = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            redirect_idx = i;
            break;
        }
    }
    
    // 輸出文本到終端
    int text_end = (redirect_idx > 0) ? redirect_idx : argc;
    for (int i = 1; i < text_end; i++) {
        printf("%s", argv[i]);
        if (i < text_end - 1) {
            printf(" ");
        }
    }
    
    if (redirect_idx < 0) {
        // 無重定向，直接換行
        printf("\n");
    } else {
        // 有重定向，寫入檔案
        const char *filename = argv[redirect_idx + 1];
        char *full_path = shell_get_full_path(shell, filename);
        if (full_path != NULL) {
            // 構建要寫入的內容
            size_t content_len = 0;
            for (int i = 1; i < redirect_idx; i++) {
                content_len += strlen(argv[i]) + 1;
            }
            
            char *content = (char *)safe_malloc(content_len + 1);
            if (content != NULL) {
                content[0] = '\0';
                for (int i = 1; i < redirect_idx; i++) {
                    strncat(content, argv[i], content_len - strlen(content));
                    if (i < redirect_idx - 1) {
                        strncat(content, " ", content_len - strlen(content));
                    }
                }
                
                // 寫入或創建檔案
                vfs_node_t *file = vfs_find_node(shell->vfs, full_path);
                if (file == NULL) {
                    file = vfs_create_file(shell->vfs, full_path, content, strlen(content));
                } else {
                    vfs_write_file(file, content, strlen(content));
                }
                
                safe_free(content);
            }
            safe_free(full_path);
        }
    }
    
    return true;
}

bool cmd_rm(shell_t *shell, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: rm <檔案名稱>\n");
        printf("      rm -r <目錄名稱>  (遞迴刪除目錄)\n");
        return false;
    }
    
    // 解析 -r 選項
    bool recursive = false;
    const char *target = argv[1];
    
    if (argc >= 3 && strcmp(argv[1], "-r") == 0) {
        recursive = true;
        target = argv[2];
    }
    
    char *full_path = shell_get_full_path(shell, target);
    if (full_path == NULL) {
        printf("錯誤: 無法解析路徑\n");
        return false;
    }
    
    // 檢查目標節點
    vfs_node_t *node = vfs_find_node(shell->vfs, full_path);
    if (node == NULL) {
        printf("錯誤: 檔案或目錄不存在\n");
        safe_free(full_path);
        return false;
    }
    
    // 目錄需要 -r 選項才能刪除
    if (node->type == VFS_DIR && !recursive) {
        printf("錯誤: '%s' 是目錄，請使用 rm -r 來刪除目錄\n", target);
        safe_free(full_path);
        return false;
    }
    
    bool result = vfs_delete_node(shell->vfs, full_path);
    safe_free(full_path);
    
    if (!result) {
        error_t err = error_get();
        if (err.code != ERR_OK) {
            printf("錯誤: %s\n", err.message);
            error_clear();
        } else {
            printf("錯誤: 無法刪除\n");
        }
        return false;
    }
    
    return true;
}

bool cmd_mv(shell_t *shell, int argc, char **argv) {
    if (argc < 3) {
        printf("用法: mv <源路徑> <目標路徑>\n");
        return false;
    }
    
    // 解析源路徑
    char *src_path = shell_get_full_path(shell, argv[1]);
    if (src_path == NULL) {
        printf("錯誤: 無法解析源路徑\n");
        return false;
    }
    
    vfs_node_t *src_node = vfs_find_node(shell->vfs, src_path);
    if (src_node == NULL) {
        printf("錯誤: 源路徑不存在\n");
        safe_free(src_path);
        return false;
    }
    
    // 解析目標路徑
    char *dst_path = shell_get_full_path(shell, argv[2]);
    if (dst_path == NULL) {
        printf("錯誤: 無法解析目標路徑\n");
        safe_free(src_path);
        return false;
    }
    
    // 如果目標是目錄，則移動到該目錄下
    vfs_node_t *dst_node = vfs_find_node(shell->vfs, dst_path);
    if (dst_node != NULL && dst_node->type == VFS_DIR) {
        char *new_dst_path = (char *)safe_malloc(strlen(dst_path) + strlen(src_node->name) + 2);
        if (new_dst_path == NULL) {
            safe_free(src_path);
            safe_free(dst_path);
            return false;
        }
        strcpy(new_dst_path, dst_path);
        if (dst_path[strlen(dst_path) - 1] != '/') {
            strcat(new_dst_path, "/");
        }
        strcat(new_dst_path, src_node->name);
        safe_free(dst_path);
        dst_path = new_dst_path;
    }
    
    bool result = vfs_move_node(shell->vfs, src_path, dst_path);
    safe_free(src_path);
    safe_free(dst_path);
    
    if (!result) {
        error_t err = error_get();
        if (err.code != ERR_OK) {
            printf("錯誤: %s\n", err.message);
            error_clear();
        } else {
            printf("錯誤: 無法移動\n");
        }
        return false;
    }
    
    return true;
}

bool cmd_cp(shell_t *shell, int argc, char **argv) {
    if (argc < 3) {
        printf("用法: cp <源路徑> <目標路徑>\n");
        return false;
    }
    
    // 解析源路徑
    char *src_path = shell_get_full_path(shell, argv[1]);
    if (src_path == NULL) {
        printf("錯誤: 無法解析源路徑\n");
        return false;
    }
    
    vfs_node_t *src = vfs_find_node(shell->vfs, src_path);
    if (src == NULL) {
        printf("錯誤: 源路徑不存在\n");
        safe_free(src_path);
        return false;
    }
    
    // 解析目標路徑
    char *dst_path = shell_get_full_path(shell, argv[2]);
    if (dst_path == NULL) {
        printf("錯誤: 無法解析目標路徑\n");
        safe_free(src_path);
        return false;
    }
    
    // 如果目標是目錄，則複製到該目錄下
    vfs_node_t *dst_node = vfs_find_node(shell->vfs, dst_path);
    if (dst_node != NULL && dst_node->type == VFS_DIR) {
        char *new_dst_path = (char *)safe_malloc(strlen(dst_path) + strlen(src->name) + 2);
        if (new_dst_path == NULL) {
            safe_free(src_path);
            safe_free(dst_path);
            return false;
        }
        strcpy(new_dst_path, dst_path);
        if (dst_path[strlen(dst_path) - 1] != '/') {
            strcat(new_dst_path, "/");
        }
        strcat(new_dst_path, src->name);
        safe_free(dst_path);
        dst_path = new_dst_path;
    }
    
    bool result = copy_node_recursive(shell->vfs, src, dst_path);
    safe_free(src_path);
    safe_free(dst_path);
    
    if (!result) {
        error_t err = error_get();
        if (err.code != ERR_OK) {
            printf("錯誤: %s\n", err.message);
            error_clear();
        } else {
            printf("錯誤: 無法複製\n");
        }
        return false;
    }
    
    return true;
}

/* ============================================================================
 * 編輯器命令實作
 * ============================================================================ */

bool cmd_vim(shell_t *shell, int argc, char **argv) {
    if (argc < 2) {
        printf("用法: vim <檔案名稱>\n");
        return false;
    }
    
    char *full_path = shell_get_full_path(shell, argv[1]);
    if (full_path == NULL) {
        printf("錯誤: 無法解析路徑\n");
        return false;
    }
    
    // 創建臨時檔案名稱（將路徑中的 / 替換為 _）
    char *tmp_filename = (char *)safe_malloc(strlen(full_path) + 10);
    if (tmp_filename == NULL) {
        safe_free(full_path);
        return false;
    }
    strcpy(tmp_filename, ".tmp_");
    for (size_t i = 0; i < strlen(full_path); i++) {
        if (full_path[i] == '/') {
            tmp_filename[strlen(tmp_filename)] = '_';
        } else {
            tmp_filename[strlen(tmp_filename)] = full_path[i];
        }
    }
    tmp_filename[strlen(tmp_filename)] = '\0';
    
    // 從 VFS 讀取檔案內容到臨時檔案
    vfs_node_t *file = vfs_find_node(shell->vfs, full_path);
    if (file != NULL && file->type == VFS_FILE) {
        size_t size = 0;
        void *data = vfs_read_file(file, &size);
        FILE *tmp_file = fopen(tmp_filename, "w");
        if (tmp_file != NULL) {
            if (data != NULL && size > 0) {
                fwrite(data, 1, size, tmp_file);
            }
            fclose(tmp_file);
            if (data != NULL) {
                safe_free(data);
            }
        } else if (data != NULL) {
            safe_free(data);
        }
    } else {
        // 檔案不存在，創建空的臨時檔案
        FILE *tmp_file = fopen(tmp_filename, "w");
        if (tmp_file != NULL) {
            fclose(tmp_file);
        }
    }
    
    // 創建並運行編輯器
    editor_t *editor = editor_create();
    if (editor == NULL) {
        printf("錯誤: 無法創建編輯器\n");
        safe_free(tmp_filename);
        safe_free(full_path);
        return false;
    }
    
    if (editor_open_file(editor, tmp_filename)) {
        editor_run(editor);
        
        // 編輯完成後，將臨時檔案內容保存回 VFS
        FILE *saved_file = fopen(tmp_filename, "r");
        if (saved_file != NULL) {
            fseek(saved_file, 0, SEEK_END);
            long size = ftell(saved_file);
            fseek(saved_file, 0, SEEK_SET);
            
            if (size > 0) {
                void *data = safe_malloc(size);
                if (data != NULL) {
                    fread(data, 1, size, saved_file);
                    
                    if (file == NULL) {
                        vfs_create_file(shell->vfs, full_path, data, size);
                    } else {
                        vfs_write_file(file, data, size);
                    }
                    
                    safe_free(data);
                }
            } else {
                // 空檔案處理
                if (file == NULL) {
                    vfs_create_file(shell->vfs, full_path, NULL, 0);
                } else {
                    vfs_write_file(file, NULL, 0);
                }
            }
            
            fclose(saved_file);
        }
        
        // 清理臨時檔案
        remove(tmp_filename);
    }
    
    editor_destroy(editor);
    safe_free(tmp_filename);
    safe_free(full_path);
    
    return true;
}

/* ============================================================================
 * Shell 控制命令實作
 * ============================================================================ */

bool cmd_clear(shell_t *shell, int argc, char **argv) {
    (void)shell;
    (void)argc;
    (void)argv;
    
    // 使用 ANSI escape code 清屏並移動游標到左上角
    printf("\033[2J\033[H");
    return true;
}

bool cmd_help(shell_t *shell, int argc, char **argv) {
    (void)shell;
    (void)argc;
    (void)argv;
    
    printf("可用命令:\n");
    printf("  ls [目錄]     - 列出目錄內容\n");
    printf("  cd [目錄]     - 切換目錄\n");
    printf("  pwd           - 顯示當前目錄\n");
    printf("  mkdir <目錄>  - 創建目錄\n");
    printf("  touch <檔案>  - 創建檔案\n");
    printf("  cat <檔案>    - 顯示檔案內容\n");
    printf("  echo [文本]   - 輸出文本（支持 > 重定向）\n");
    printf("  rm <檔案>     - 刪除檔案\n");
    printf("  rm -r <目錄>  - 遞迴刪除目錄\n");
    printf("  mv <源> <目標> - 移動/重命名\n");
    printf("  cp <源> <目標> - 複製檔案或目錄\n");
    printf("  vim <檔案>    - 使用編輯器打開檔案\n");
    printf("  clear         - 清屏\n");
    printf("  history       - 顯示歷史記錄\n");
    printf("  help          - 顯示幫助\n");
    printf("  exit          - 退出\n");
    return true;
}

bool cmd_history(shell_t *shell, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (shell->history_count == 0) {
        printf("(無歷史記錄)\n");
        return true;
    }
    
    // 輸出所有歷史記錄，格式：編號 + 命令
    for (int i = 0; i < shell->history_count; i++) {
        printf("%4d  %s\n", i + 1, shell->history[i]);
    }
    return true;
}

bool cmd_exit(shell_t *shell, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    shell->running = false;
    return true;
}

