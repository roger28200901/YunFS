/**
 * @file shell_completion.c
 * @brief Shell Tab 自動完成模組實作
 * 
 * 實作命令列的 Tab 自動完成功能和進階輸入處理。
 */

#include "shell_completion.h"
#include "shell_commands.h"
#include "../filesystem/vfs.h"
#include "../utils/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* ============================================================================
 * 補全功能實作
 * ============================================================================ */

char **shell_get_completions(shell_t *shell, const char *prefix, size_t *count) {
    *count = 0;
    
    // 分離目錄路徑和檔名前綴
    // 例如: "dir/sub/fi" -> dir_path="dir/sub/", name_prefix="fi"
    const char *last_slash = strrchr(prefix, '/');
    char *dir_path = NULL;
    const char *name_prefix = prefix;
    
    if (last_slash != NULL) {
        size_t dir_len = last_slash - prefix + 1;
        dir_path = (char *)safe_malloc(dir_len + 1);
        if (dir_path == NULL) return NULL;
        strncpy(dir_path, prefix, dir_len);
        dir_path[dir_len] = '\0';
        name_prefix = last_slash + 1;
    }
    
    // 確定要搜尋的目錄
    vfs_node_t *search_dir;
    if (dir_path != NULL) {
        char *full_dir = shell_get_full_path(shell, dir_path);
        if (full_dir == NULL) {
            safe_free(dir_path);
            return NULL;
        }
        search_dir = vfs_find_node(shell->vfs, full_dir);
        safe_free(full_dir);
        if (search_dir == NULL || search_dir->type != VFS_DIR) {
            safe_free(dir_path);
            return NULL;
        }
    } else {
        search_dir = shell->current_dir;
    }
    
    // 動態陣列收集符合前綴的項目
    size_t prefix_len = strlen(name_prefix);
    size_t capacity = 16;
    char **completions = (char **)safe_malloc(sizeof(char *) * capacity);
    if (completions == NULL) {
        safe_free(dir_path);
        return NULL;
    }
    
    // 遍歷目錄中的所有子節點
    vfs_node_t *child = search_dir->children;
    while (child != NULL) {
        // 檢查是否符合前綴
        if (prefix_len == 0 || strncmp(child->name, name_prefix, prefix_len) == 0) {
            // 擴展陣列容量
            if (*count >= capacity) {
                capacity *= 2;
                char **new_completions = (char **)realloc(completions, sizeof(char *) * capacity);
                if (new_completions == NULL) {
                    shell_free_completions(completions, *count);
                    safe_free(dir_path);
                    return NULL;
                }
                completions = new_completions;
            }
            
            // 構建完整的補全字串（包含目錄路徑）
            size_t name_len = strlen(child->name);
            size_t total_len = (dir_path ? strlen(dir_path) : 0) + name_len + 2;
            completions[*count] = (char *)safe_malloc(total_len);
            if (completions[*count] != NULL) {
                if (dir_path) {
                    strcpy(completions[*count], dir_path);
                    strcat(completions[*count], child->name);
                } else {
                    strcpy(completions[*count], child->name);
                }
                // 目錄加上 "/" 後綴以便識別
                if (child->type == VFS_DIR) {
                    strcat(completions[*count], "/");
                }
                (*count)++;
            }
        }
        child = child->next;
    }
    
    safe_free(dir_path);
    return completions;
}

void shell_free_completions(char **completions, size_t count) {
    if (completions == NULL) return;
    for (size_t i = 0; i < count; i++) {
        safe_free(completions[i]);
    }
    safe_free(completions);
}

char *shell_find_common_prefix(char **completions, size_t count) {
    if (count == 0 || completions == NULL) return NULL;
    if (count == 1) return safe_strdup(completions[0]);
    
    // 以第一個項目為基準，逐字元比較找出共同前綴
    size_t prefix_len = strlen(completions[0]);
    for (size_t i = 1; i < count; i++) {
        size_t j = 0;
        while (j < prefix_len && completions[0][j] == completions[i][j]) {
            j++;
        }
        prefix_len = j;
    }
    
    return safe_strndup(completions[0], prefix_len);
}

/* ============================================================================
 * 輸入處理實作
 * ============================================================================ */

/**
 * @brief 處理 Tab 鍵自動完成
 * @param shell Shell 實例
 * @param buffer 輸入緩衝區
 * @param pos 當前緩衝區長度指標
 * @param cursor 當前游標位置指標
 * @param size 緩衝區最大大小
 */
static void handle_tab_completion(shell_t *shell, char *buffer, 
                                   size_t *pos, size_t *cursor, size_t size) {
    // 找到當前正在輸入的詞的起始位置
    size_t word_start = *cursor;
    while (word_start > 0 && buffer[word_start - 1] != ' ') {
        word_start--;
    }
    
    // 提取當前詞
    char word[256];
    size_t word_len = *cursor - word_start;
    if (word_len >= sizeof(word)) return;
    
    strncpy(word, buffer + word_start, word_len);
    word[word_len] = '\0';
    
    // 獲取補全選項
    size_t comp_count = 0;
    char **completions = shell_get_completions(shell, word, &comp_count);
    
    if (comp_count == 1) {
        // 唯一匹配：直接補全
        size_t comp_len = strlen(completions[0]);
        size_t add_len = comp_len - word_len;
        
        if (add_len > 0 && *pos + add_len < size - 1) {
            // 移動游標後的內容，插入補全部分
            memmove(buffer + *cursor + add_len, buffer + *cursor, *pos - *cursor + 1);
            memcpy(buffer + *cursor, completions[0] + word_len, add_len);
            *pos += add_len;
            
            // 更新顯示
            printf("%s", buffer + *cursor);
            *cursor += add_len;
            if (*pos > *cursor) {
                printf("\033[%zuD", *pos - *cursor);
            }
        }
    } else if (comp_count > 1) {
        // 多個匹配：嘗試補全共同前綴
        char *common = shell_find_common_prefix(completions, comp_count);
        if (common != NULL) {
            size_t common_len = strlen(common);
            if (common_len > word_len) {
                // 有共同前綴可以補全
                size_t add_len = common_len - word_len;
                if (*pos + add_len < size - 1) {
                    memmove(buffer + *cursor + add_len, buffer + *cursor, *pos - *cursor + 1);
                    memcpy(buffer + *cursor, common + word_len, add_len);
                    *pos += add_len;
                    
                    printf("%s", buffer + *cursor);
                    *cursor += add_len;
                    if (*pos > *cursor) {
                        printf("\033[%zuD", *pos - *cursor);
                    }
                }
            } else {
                // 無法進一步補全，顯示所有選項
                printf("\n");
                for (size_t i = 0; i < comp_count; i++) {
                    printf("%s  ", completions[i]);
                }
                printf("\n");
                
                // 重新顯示提示符和當前輸入
                char *current_path = vfs_get_path(shell->current_dir);
                if (current_path != NULL) {
                    printf("\033[32m%s\033[0m ", current_path);
                    safe_free(current_path);
                }
                printf("%s%s", shell->prompt, buffer);
                if (*pos > *cursor) {
                    printf("\033[%zuD", *pos - *cursor);
                }
            }
            safe_free(common);
        }
    }
    
    shell_free_completions(completions, comp_count);
    fflush(stdout);
}

bool shell_read_line_with_completion(shell_t *shell, char *buffer, size_t size) {
    // 設置終端為原始模式（關閉行緩衝和回顯）
    struct termios old_termios, new_termios;
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    
    size_t pos = 0;     // 緩衝區中的字元數
    size_t cursor = 0;  // 游標位置
    buffer[0] = '\0';
    
    while (pos < size - 1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
            return false;
        }
        
        if (c == '\n' || c == '\r') {
            // Enter: 結束輸入
            printf("\n");
            buffer[pos] = '\0';
            tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
            return true;
        } else if (c == '\t') {
            // Tab: 自動完成
            handle_tab_completion(shell, buffer, &pos, &cursor, size);
        } else if (c == 127 || c == '\b') {
            // Backspace: 刪除前一個字元
            if (cursor > 0) {
                memmove(buffer + cursor - 1, buffer + cursor, pos - cursor + 1);
                pos--;
                cursor--;
                printf("\b%s \b", buffer + cursor);
                if (pos > cursor) {
                    printf("\033[%zuD", pos - cursor);
                }
                fflush(stdout);
            }
        } else if (c == '\x1b') {
            // Escape 序列（方向鍵等）
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) == 1 && seq[0] == '[') {
                if (read(STDIN_FILENO, &seq[1], 1) == 1) {
                    if (seq[1] == 'D' && cursor > 0) {
                        // 左方向鍵
                        cursor--;
                        printf("\033[D");
                        fflush(stdout);
                    } else if (seq[1] == 'C' && cursor < pos) {
                        // 右方向鍵
                        cursor++;
                        printf("\033[C");
                        fflush(stdout);
                    }
                    // 可擴展：上下鍵用於歷史記錄瀏覽
                }
            }
        } else if (c == '\x01') {
            // Ctrl+A: 移到行首
            if (cursor > 0) {
                printf("\033[%zuD", cursor);
                cursor = 0;
                fflush(stdout);
            }
        } else if (c == '\x05') {
            // Ctrl+E: 移到行尾
            if (cursor < pos) {
                printf("\033[%zuC", pos - cursor);
                cursor = pos;
                fflush(stdout);
            }
        } else if (c == '\x15') {
            // Ctrl+U: 清除整行
            while (cursor > 0) {
                printf("\b");
                cursor--;
            }
            printf("\033[K");
            pos = 0;
            buffer[0] = '\0';
            fflush(stdout);
        } else if (c >= 32 && c < 127) {
            // 可列印字元：插入到游標位置
            memmove(buffer + cursor + 1, buffer + cursor, pos - cursor + 1);
            buffer[cursor] = c;
            pos++;
            cursor++;
            printf("%s", buffer + cursor - 1);
            if (pos > cursor) {
                printf("\033[%zuD", pos - cursor);
            }
            fflush(stdout);
        }
    }
    
    buffer[size - 1] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    return true;
}

