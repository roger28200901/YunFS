/**
 * @file screen.c
 * @brief 螢幕渲染模組實作
 *
 * 實作終端機螢幕的控制與渲染功能。
 * 使用 ANSI 跳脫序列控制游標與顏色。
 *
 * @author Yun
 * @date 2025
 */

#include "screen.h"
#include "../utils/error.h"
#include "colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* ============================================================================
 * 模組內部狀態
 * ============================================================================ */

/** 螢幕尺寸（預設值） */
static screen_size_t g_screen_size = {24, 80};

/** 螢幕是否已初始化 */
static bool g_screen_initialized = false;

/** 螢幕緩衝區（用於雙緩衝渲染） */
static char **g_screen_buffer = NULL;

/** 緩衝區行數 */
static size_t g_screen_rows = 0;

/* ============================================================================
 * 螢幕初始化與清理
 * ============================================================================ */

/**
 * @brief 初始化螢幕系統
 */
bool screen_init(void) {
    screen_size_t size = screen_get_size();
    g_screen_size = size;
    g_screen_initialized = true;
    
    /* 配置螢幕緩衝區 */
    g_screen_rows = size.rows;
    g_screen_buffer = (char **)calloc(g_screen_rows, sizeof(char *));
    
    return true;
}

/**
 * @brief 清理螢幕系統
 */
void screen_cleanup(void) {
    /* 釋放螢幕緩衝區 */
    if (g_screen_buffer != NULL) {
        for (size_t i = 0; i < g_screen_rows; i++) {
            free(g_screen_buffer[i]);
        }
        free(g_screen_buffer);
        g_screen_buffer = NULL;
    }
    
    /* 恢復終端機設定 */
    printf("\033[?25h");       /* 顯示游標 */
    printf("\033[0m");         /* 重置顏色 */
    printf("\033[2J\033[H");   /* 清除螢幕並移至左上角 */
    fflush(stdout);
    g_screen_initialized = false;
}

/* ============================================================================
 * 螢幕資訊與控制
 * ============================================================================ */

/**
 * @brief 取得螢幕尺寸
 */
screen_size_t screen_get_size(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        g_screen_size.rows = w.ws_row;
        g_screen_size.cols = w.ws_col;
    }
    return g_screen_size;
}

/**
 * @brief 清除螢幕
 */
void screen_clear(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

/**
 * @brief 設定游標位置
 */
void screen_set_cursor(size_t row, size_t col) {
    printf("\033[%zu;%zuH", row + 1, col + 1);
    fflush(stdout);
}

/**
 * @brief 隱藏游標
 */
void screen_hide_cursor(void) {
    printf("\033[?25l");
    fflush(stdout);
}

/**
 * @brief 顯示游標
 */
void screen_show_cursor(void) {
    printf("\033[?25h");
    fflush(stdout);
}

/* ============================================================================
 * 內容渲染
 * ============================================================================ */

/**
 * @brief 重新繪製螢幕
 */
void screen_refresh(buffer_t *buf, cursor_t *cursor, size_t first_line) {
    if (buf == NULL || cursor == NULL) {
        return;
    }
    
    screen_size_t size = screen_get_size();
    
    /* 清除螢幕並移至左上角 */
    printf("\033[2J\033[H");
    
    /* 計算可顯示的行數（保留狀態列與命令列） */
    size_t display_rows = size.rows - 2;
    if (display_rows < 1) {
        display_rows = 1;
    }
    
    /* 調整捲動位置，確保游標可見 */
    if (cursor->row < first_line) {
        first_line = cursor->row;
    } else if (cursor->row >= first_line + display_rows) {
        first_line = cursor->row - display_rows + 1;
    }
    
    /* 若檔案行數少於顯示行數，從第一行開始 */
    if (buf->line_count < display_rows) {
        first_line = 0;
        display_rows = buf->line_count;
    }
    
    size_t last_line = first_line + display_rows;
    if (last_line > buf->line_count) {
        last_line = buf->line_count;
    }
    
    /* 逐行渲染 */
    size_t line_num = first_line;
    line_t *line = buffer_get_line(buf, first_line);
    
    for (size_t row = 0; row < display_rows && line != NULL; row++) {
        /* 顯示行號（灰色，右對齊） */
        printf("\033[90m%4zu\033[0m ", line_num + 1);
        
        /* 顯示行內容 */
        size_t col = 0;
        size_t line_len = line->length;
        size_t max_col = size.cols - 6;  /* 保留行號空間 */
        
        bool is_cursor_line = (line_num == cursor->row);
        
        for (size_t i = 0; i < line_len && col < max_col; i++) {
            char c = line->text[i];
            
            if (is_cursor_line && col == cursor->col) {
                /* 游標位置：反色顯示 */
                printf("\033[30;47m%c\033[0m", c);
            } else {
                printf("%c", c);
            }
            col++;
        }
        
        /* 若游標在行尾之後 */
        if (is_cursor_line && cursor->col >= line_len && col < max_col) {
            printf("\033[30;47m \033[0m");
        }
        
        /* 填充剩餘空間 */
        while (col < max_col) {
            printf(" ");
            col++;
        }
        
        printf("\r\n");
        
        line = line->next;
        line_num++;
    }
    
    fflush(stdout);
}

/**
 * @brief 顯示狀態列
 */
void screen_show_status(const char *status, bool is_error) {
    screen_size_t size = screen_get_size();
    
    /* 移至狀態列位置 */
    screen_set_cursor(size.rows - 2, 0);
    
    /* 清除狀態列並設定藍色背景 */
    printf("\033[44m");
    for (size_t i = 0; i < size.cols; i++) {
        printf(" ");
    }
    screen_set_cursor(size.rows - 2, 0);
    
    /* 根據是否為錯誤選擇文字顏色 */
    if (is_error) {
        printf("\033[91m%s\033[0m", status ? status : "");  /* 紅色 */
    } else {
        printf("\033[97m%s\033[0m", status ? status : "");  /* 白色 */
    }
    
    printf("\033[0m");
    fflush(stdout);
}

/**
 * @brief 顯示命令列
 */
void screen_show_command(const char *command) {
    screen_size_t size = screen_get_size();
    
    /* 移至命令列位置 */
    screen_set_cursor(size.rows - 1, 0);
    
    /* 清除命令列 */
    printf("\033[0m");
    for (size_t i = 0; i < size.cols; i++) {
        printf(" ");
    }
    screen_set_cursor(size.rows - 1, 0);
    
    /* 顯示命令（青色） */
    if (command != NULL && strlen(command) > 0) {
        printf("\033[36m:%s\033[0m", command);
    } else {
        printf("\033[36m:\033[0m");
    }
    
    fflush(stdout);
}
