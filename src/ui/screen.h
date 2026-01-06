/**
 * @file screen.h
 * @brief 螢幕渲染模組標頭檔
 *
 * 本模組負責終端機螢幕的控制與渲染，包含：
 * - 螢幕尺寸偵測
 * - 游標位置控制
 * - 緩衝區內容渲染
 * - 狀態列與命令列顯示
 *
 * @author Yun
 * @date 2025
 */

#ifndef SCREEN_H
#define SCREEN_H

#include <stddef.h>
#include <stdbool.h>
#include "../core/buffer.h"

/* ============================================================================
 * 資料結構定義
 * ============================================================================ */

/**
 * @brief 游標位置結構
 */
typedef struct cursor {
    size_t row;    /**< 行號（從 0 開始） */
    size_t col;    /**< 列號（從 0 開始） */
} cursor_t;

/**
 * @brief 螢幕尺寸結構
 */
typedef struct {
    size_t rows;   /**< 螢幕行數 */
    size_t cols;   /**< 螢幕列數 */
} screen_size_t;

/* ============================================================================
 * 螢幕初始化與清理
 * ============================================================================ */

/**
 * @brief 初始化螢幕系統
 *
 * 偵測螢幕尺寸並配置內部緩衝區。
 *
 * @return true 成功，false 失敗
 */
bool screen_init(void);

/**
 * @brief 清理螢幕系統
 *
 * 釋放內部緩衝區，恢復游標顯示與終端機設定。
 */
void screen_cleanup(void);

/* ============================================================================
 * 螢幕資訊與控制
 * ============================================================================ */

/**
 * @brief 取得螢幕尺寸
 *
 * @return 目前的螢幕尺寸
 */
screen_size_t screen_get_size(void);

/**
 * @brief 清除螢幕
 *
 * 清除螢幕內容並將游標移至左上角。
 */
void screen_clear(void);

/**
 * @brief 設定游標位置
 *
 * @param row 目標行號（從 0 開始）
 * @param col 目標列號（從 0 開始）
 */
void screen_set_cursor(size_t row, size_t col);

/**
 * @brief 隱藏游標
 */
void screen_hide_cursor(void);

/**
 * @brief 顯示游標
 */
void screen_show_cursor(void);

/* ============================================================================
 * 內容渲染
 * ============================================================================ */

/**
 * @brief 重新繪製螢幕
 *
 * 根據緩衝區內容與游標位置重新繪製整個螢幕。
 *
 * @param buf        文字緩衝區
 * @param cursor     游標位置
 * @param first_line 顯示的起始行號（用於捲動）
 */
void screen_refresh(buffer_t *buf, cursor_t *cursor, size_t first_line);

/**
 * @brief 顯示狀態列
 *
 * 在螢幕底部顯示狀態訊息。
 *
 * @param status   狀態訊息字串
 * @param is_error 是否為錯誤訊息（影響顯示顏色）
 */
void screen_show_status(const char *status, bool is_error);

/**
 * @brief 顯示命令列
 *
 * 在螢幕最底部顯示命令輸入區。
 *
 * @param command 目前輸入的命令字串
 */
void screen_show_command(const char *command);

#endif // SCREEN_H
