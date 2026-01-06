/**
 * @file splash.c
 * @brief 啟動畫面模組實作
 *
 * 實作具有視覺特效的啟動畫面，靈感來自 Neovim。
 *
 * @author Yun
 * @date 2025
 */

#include "splash.h"
#include "colors.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* ============================================================================
 * ANSI 顏色定義
 * ============================================================================ */

#define C_RESET     "\033[0m"
#define C_BOLD      "\033[1m"
#define C_DIM       "\033[2m"
#define C_CYAN      "\033[36m"
#define C_MAGENTA   "\033[35m"
#define C_BLUE      "\033[34m"
#define C_GREEN     "\033[32m"
#define C_YELLOW    "\033[33m"
#define C_WHITE     "\033[97m"
#define C_GRAY      "\033[90m"

/* 漸層色 */
#define G1 "\033[38;5;51m"   /* 亮青色 */
#define G2 "\033[38;5;50m"
#define G3 "\033[38;5;49m"
#define G4 "\033[38;5;48m"
#define G5 "\033[38;5;47m"   /* 亮綠色 */
#define G6 "\033[38;5;46m"

/* ============================================================================
 * ASCII Art 標題
 * ============================================================================ */

static const char *logo[] = {
    "",
    G1 "  ██╗   ██╗██╗   ██╗███╗   ██╗███████╗██╗██╗     ███████╗" C_RESET,
    G2 "  ╚██╗ ██╔╝██║   ██║████╗  ██║██╔════╝██║██║     ██╔════╝" C_RESET,
    G3 "   ╚████╔╝ ██║   ██║██╔██╗ ██║█████╗  ██║██║     █████╗  " C_RESET,
    G4 "    ╚██╔╝  ██║   ██║██║╚██╗██║██╔══╝  ██║██║     ██╔══╝  " C_RESET,
    G5 "     ██║   ╚██████╔╝██║ ╚████║██║     ██║███████╗███████╗" C_RESET,
    G6 "     ╚═╝    ╚═════╝ ╚═╝  ╚═══╝╚═╝     ╚═╝╚══════╝╚══════╝" C_RESET,
    "",
    C_CYAN C_BOLD "           ╔═══════════════════════════════════════╗" C_RESET,
    C_CYAN        "           ║" C_WHITE C_BOLD " VIRTUAL FILE SYSTEM " C_CYAN "                  ║" C_RESET,
    C_CYAN C_BOLD "           ╚═══════════════════════════════════════╝" C_RESET,
    "",
    NULL
};

static const char *info[] = {
    C_GRAY "  ┌─────────────────────────────────────────────────────────────" C_RESET,
    C_GRAY "  │" C_GREEN  "Only young man can using this file system" C_RESET,
    C_GRAY "  │" C_CYAN   "If you are not a young man, please leave"  C_RESET,
    C_GRAY "  │" C_YELLOW "Author: YunHong Chen (roger28200901@gmail.com)" C_RESET,
    C_GRAY "  └─────────────────────────────────────────────────────────────" C_RESET,
    "",
    C_DIM "                    Type " C_WHITE "'help'" C_DIM " to see available commands" C_RESET,
    C_DIM "                    Type " C_WHITE "'exit'" C_DIM " to quit the shell" C_RESET,
    "",
    NULL
};

/* ============================================================================
 * 輔助函式
 * ============================================================================ */

/**
 * @brief 取得終端機寬度
 */
static int get_term_width(void) {
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80;
}

/**
 * @brief 清除螢幕
 */
static void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

/**
 * @brief 延遲顯示（毫秒）
 */
static void delay_ms(int ms) {
    usleep(ms * 1000);
}

/**
 * @brief 逐行動畫顯示
 */
static void animate_lines(const char **lines, int delay) {
    for (int i = 0; lines[i] != NULL; i++) {
        printf("%s\n", lines[i]);
        fflush(stdout);
        delay_ms(delay);
    }
}

/* ============================================================================
 * 公開函式
 * ============================================================================ */

void splash_show(void) {
    int width = get_term_width();
    (void)width;  /* 未來可用於置中 */
    
    clear_screen();
    
    /* 顯示 Logo（帶動畫） */
    animate_lines(logo, 30);
    
    /* 稍作停頓 */
    delay_ms(200);
    
    /* 顯示資訊區塊 */
    animate_lines(info, 40);
    
    /* 最終停頓 */
    delay_ms(300);
    
    printf("\n");
}

