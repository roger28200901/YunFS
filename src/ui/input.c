/**
 * @file input.c
 * @brief 鍵盤輸入處理模組實作
 *
 * 實作終端機原始模式設定與鍵盤輸入處理。
 *
 * @author Yun
 * @date 2025
 */

#include "input.h"
#include "../utils/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

/* ============================================================================
 * 模組內部狀態
 * ============================================================================ */

/** 原始終端機設定（用於恢復） */
static struct termios g_original_termios;

/** 終端機是否已初始化 */
static bool g_terminal_initialized = false;

/* ============================================================================
 * 終端機初始化與清理
 * ============================================================================ */

/**
 * @brief 初始化輸入系統
 */
bool input_init(void) {
    if (g_terminal_initialized) {
        return true;
    }
    
    /* 儲存原始終端機設定 */
    if (tcgetattr(STDIN_FILENO, &g_original_termios) == -1) {
        error_set(ERR_IO_ERROR, "無法獲取終端設置");
        return false;
    }
    
    /* 設定原始模式 */
    struct termios raw = g_original_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;   /* 至少讀取 1 個字元 */
    raw.c_cc[VTIME] = 0;  /* 無逾時 */
    
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        error_set(ERR_IO_ERROR, "無法設置終端為原始模式");
        return false;
    }
    
    g_terminal_initialized = true;
    return true;
}

/**
 * @brief 清理輸入系統
 */
void input_cleanup(void) {
    if (g_terminal_initialized) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_original_termios);
        g_terminal_initialized = false;
    }
}

/* ============================================================================
 * 按鍵讀取函式
 * ============================================================================ */

/**
 * @brief 讀取單一按鍵輸入
 */
bool input_read_key(key_input_t *key) {
    if (key == NULL) {
        return false;
    }
    
    /* 初始化按鍵結構 */
    memset(key, 0, sizeof(key_input_t));
    
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) {
        return false;
    }
    
    /* 處理 Escape 序列 */
    if (c == '\x1b') {
        key->escape = true;
        char seq[3];
        
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return false;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return false;
        
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return false;
                if (seq[2] == '~') {
                    /* 處理功能鍵 */
                    switch (seq[1]) {
                        case '1': key->key = 'H'; break;  /* Home */
                        case '3': key->key = 'D'; break;  /* Delete */
                        case '4': key->key = 'F'; break;  /* End */
                        case '5': key->key = 'P'; break;  /* Page Up */
                        case '6': key->key = 'N'; break;  /* Page Down */
                        default: key->key = c; break;
                    }
                }
            } else {
                /* 處理方向鍵 */
                switch (seq[1]) {
                    case 'A': key->key = 'k'; break;  /* 上 */
                    case 'B': key->key = 'j'; break;  /* 下 */
                    case 'C': key->key = 'l'; break;  /* 右 */
                    case 'D': key->key = 'h'; break;  /* 左 */
                    case 'H': key->key = 'H'; break;  /* Home */
                    case 'F': key->key = 'F'; break;  /* End */
                    default: key->key = c; break;
                }
            }
            memcpy(key->escape_seq, seq, 3);
            key->escape_seq[3] = '\0';
        } else if (seq[0] == 'O') {
            /* 處理 xterm 風格的功能鍵 */
            switch (seq[1]) {
                case 'H': key->key = 'H'; break;  /* Home */
                case 'F': key->key = 'F'; break;  /* End */
                default: key->key = c; break;
            }
            memcpy(key->escape_seq, seq, 2);
            key->escape_seq[2] = '\0';
        } else {
            key->key = c;
        }
    } else if (c == '\r' || c == '\n') {
        key->key = '\n';
    } else if (c == '\x7f') {
        key->key = '\b';  /* Backspace */
    } else if (c < 32) {
        /* 處理控制字元 */
        switch (c) {
            case '\x01': key->ctrl = true; key->key = 'a'; break;  /* Ctrl+A */
            case '\x03': key->ctrl = true; key->key = 'c'; break;  /* Ctrl+C */
            case '\x05': key->ctrl = true; key->key = 'e'; break;  /* Ctrl+E */
            case '\x06': key->ctrl = true; key->key = 'f'; break;  /* Ctrl+F */
            case '\x08': key->ctrl = true; key->key = 'h'; break;  /* Ctrl+H */
            case '\x0b': key->ctrl = true; key->key = 'k'; break;  /* Ctrl+K */
            case '\x0c': key->ctrl = true; key->key = 'l'; break;  /* Ctrl+L */
            case '\x15': key->ctrl = true; key->key = 'u'; break;  /* Ctrl+U */
            case '\x17': key->ctrl = true; key->key = 'w'; break;  /* Ctrl+W */
            default: key->key = c; break;
        }
    } else {
        key->key = c;
    }
    
    return true;
}

/**
 * @brief 讀取一行輸入
 */
bool input_read_line(char *buffer, size_t size) {
    if (buffer == NULL || size == 0) {
        return false;
    }
    
    size_t pos = 0;
    buffer[0] = '\0';
    
    while (pos < size - 1) {
        key_input_t key;
        if (!input_read_key(&key)) {
            continue;
        }
        
        if (key.key == '\n' || key.key == '\r') {
            /* Enter：完成輸入 */
            buffer[pos] = '\0';
            return true;
        } else if (key.key == '\b' || (key.ctrl && key.key == 'h')) {
            /* Backspace：刪除前一字元 */
            if (pos > 0) {
                pos--;
                buffer[pos] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
        } else if (key.ctrl && key.key == 'u') {
            /* Ctrl+U：清除整行 */
            while (pos > 0) {
                pos--;
                printf("\b \b");
            }
            buffer[0] = '\0';
            fflush(stdout);
        } else if (key.key >= 32 && key.key < 127) {
            /* 可列印字元 */
            buffer[pos++] = key.key;
            buffer[pos] = '\0';
            printf("%c", key.key);
            fflush(stdout);
        } else if (key.key == '\x1b') {
            /* Escape：取消輸入 */
            return false;
        }
    }
    
    buffer[size - 1] = '\0';
    return true;
}

/**
 * @brief 檢查是否為特殊按鍵
 */
bool is_special_key(key_input_t *key) {
    if (key == NULL) {
        return false;
    }
    
    return key->escape || key->ctrl || key->alt;
}
