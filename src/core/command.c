/**
 * @file command.c
 * @brief 命令解析模組實作
 * 
 * 實作 Vim 風格命令的解析邏輯。
 */

#include "command.h"
#include "../utils/memory.h"
#include "../utils/error.h"
#include "../security/validation.h"
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * 命令解析實作
 * ============================================================================ */

command_t *command_parse(const char *cmd_str) {
    /* 參數驗證 */
    if (cmd_str == NULL || strlen(cmd_str) == 0) {
        return NULL;
    }
    
    /* 配置命令結構 */
    command_t *cmd = (command_t *)safe_malloc(sizeof(command_t));
    if (cmd == NULL) {
        return NULL;
    }
    
    /* 初始化預設值 */
    cmd->type = CMD_UNKNOWN;
    cmd->arg1 = NULL;
    cmd->arg2 = NULL;
    cmd->force = false;
    
    /* 跳過前導空白字元 */
    const char *p = cmd_str;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    
    /* 提取命令名稱（直到空白、斜線或字串結尾） */
    char cmd_name[32];
    size_t i = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '/' && i < sizeof(cmd_name) - 1) {
        cmd_name[i++] = *p++;
    }
    cmd_name[i] = '\0';
    
    /* 根據命令名稱判斷類型並解析參數 */
    
    if (strcmp(cmd_name, "q") == 0) {
        /* :q 或 :q! - 退出命令 */
        cmd->type = CMD_QUIT;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '!') {
            cmd->type = CMD_QUIT_FORCE;
            cmd->force = true;
        }
    } 
    else if (strcmp(cmd_name, "w") == 0) {
        /* :w 或 :wq - 儲存命令 */
        cmd->type = CMD_WRITE;
        while (*p == ' ' || *p == '\t') p++;
        
        if (*p == 'q') {
            /* :wq - 儲存並退出 */
            cmd->type = CMD_WRITE_QUIT;
        } else if (*p != '\0') {
            /* :w <filename> - 另存新檔 */
            while (*p == ' ' || *p == '\t') p++;
            if (*p != '\0') {
                size_t len = strlen(p);
                cmd->arg1 = safe_strndup(p, len);
            }
        }
    } 
    else if (strcmp(cmd_name, "wq") == 0) {
        /* :wq - 儲存並退出 */
        cmd->type = CMD_WRITE_QUIT;
    } 
    else if (strcmp(cmd_name, "e") == 0 || strcmp(cmd_name, "edit") == 0) {
        /* :e <file> 或 :edit <file> - 開啟檔案 */
        cmd->type = CMD_EDIT;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '\0') {
            size_t len = strlen(p);
            cmd->arg1 = safe_strndup(p, len);
        }
    } 
    else if (strcmp(cmd_name, "b") == 0 || strcmp(cmd_name, "buffer") == 0) {
        /* :b <n> 或 :buffer <n> - 切換緩衝區 */
        cmd->type = CMD_BUFFER;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '\0') {
            size_t len = strlen(p);
            cmd->arg1 = safe_strndup(p, len);
        }
    } 
    else if (strcmp(cmd_name, "s") == 0 || strcmp(cmd_name, "substitute") == 0) {
        /* :s/old/new/ - 搜尋替換 */
        cmd->type = CMD_SUBSTITUTE;
        
        /* 解析 /old/new/ 格式 */
        if (*p == '/') {
            p++;
            const char *old_start = p;
            
            /* 找到第一個分隔符 */
            while (*p != '\0' && *p != '/') p++;
            
            if (*p == '/') {
                size_t old_len = p - old_start;
                cmd->arg1 = safe_strndup(old_start, old_len);
                p++;
                
                const char *new_start = p;
                /* 找到第二個分隔符 */
                while (*p != '\0' && *p != '/') p++;
                size_t new_len = p - new_start;
                cmd->arg2 = safe_strndup(new_start, new_len);
            }
        }
    } 
    else if (strcmp(cmd_name, "set") == 0) {
        /* :set <option> - 設定選項 */
        cmd->type = CMD_SET;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '\0') {
            size_t len = strlen(p);
            cmd->arg1 = safe_strndup(p, len);
        }
    } 
    else if (cmd_name[0] == '/') {
        /* /pattern - 搜尋命令（以斜線開頭） */
        cmd->type = CMD_SEARCH;
        cmd->arg1 = safe_strdup(cmd_name + 1);  /* 跳過開頭的斜線 */
    }
    /* 其他情況保持 CMD_UNKNOWN */
    
    return cmd;
}

/* ============================================================================
 * 記憶體管理
 * ============================================================================ */

void command_free(command_t *cmd) {
    if (cmd == NULL) {
        return;
    }
    
    /* 釋放參數字串 */
    safe_free(cmd->arg1);
    safe_free(cmd->arg2);
    
    /* 釋放命令結構本身 */
    safe_free(cmd);
}

/* ============================================================================
 * 命令執行（預留介面）
 * ============================================================================ */

bool command_execute(command_t *cmd, void *context) {
    if (cmd == NULL) {
        return false;
    }
    
    /* 
     * 此函式為預留介面，實際的命令執行邏輯
     * 在編輯器模組（editor.c）中實作。
     */
    (void)context;
    
    return true;
}
