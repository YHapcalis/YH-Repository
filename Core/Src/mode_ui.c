/**
 * @file    mode_ui.c
 * @brief   Mode 屏 — 左侧信息 + 右侧诊断，单页显示
 *
 * 创建独立 screen，通过 mode_ui_show/hide 切换
 */

#include "mode_ui.h"
#include "lvgl.h"
#include "en25q128.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* ── 自定义中文字体 ── */
LV_FONT_DECLARE(ui_font_chinese_16);

/* ── 控件句柄 ── */
static lv_obj_t *ui_scr_mode;

/* 左侧信息列 */
static lv_obj_t *ui_label_info_flash;
static lv_obj_t *ui_label_info_sram;
static lv_obj_t *ui_label_info_heap;
static lv_obj_t *ui_label_info_ota;
static lv_obj_t *ui_label_info_uptime;
static lv_obj_t *ui_label_info_fw;
static lv_obj_t *ui_label_info_can;
static lv_obj_t *ui_label_info_f103;

/* 右侧诊断列 */
static lv_obj_t *ui_label_diag_spi;
static lv_obj_t *ui_label_diag_can;
static lv_obj_t *ui_label_diag_lcd;
static lv_obj_t *ui_label_diag_touch;
static lv_obj_t *ui_label_diag_uptime;

/* ── 前置声明 ── */
static void btn_back_cb(lv_event_t *e);

/* ═════════════════════════════════════════════════════════════
 *  创建 Mode 屏
 * ═════════════════════════════════════════════════════════════ */
void mode_ui_create(void)
{
    ui_scr_mode = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scr_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scr_mode, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_scr_mode, 255, LV_PART_MAIN);

    /* ── 顶栏：返回按钮 ── */
    lv_obj_t *top_bar = lv_obj_create(ui_scr_mode);
    lv_obj_set_size(top_bar, 800, 32);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(top_bar, 0, LV_PART_MAIN);

    /* ← 返回 */
    lv_obj_t *btn_back = lv_btn_create(top_bar);
    lv_obj_set_size(btn_back, 70, 28);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_back, 4, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, btn_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "← 返回");
    lv_obj_set_style_text_font(lbl_back, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_back, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_center(lbl_back);

    /* Mode 标题 */
    lv_obj_t *lbl_title = lv_label_create(top_bar);
    lv_label_set_text(lbl_title, "Mode");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    /* ═══════════════════════════════════════════════════════
     *  左侧：系统信息 + 连接状态 (x=10)
     * ═══════════════════════════════════════════════════════ */
    int ly = 50;   /* left column y counter */

    lv_obj_t *sec;
    sec = lv_label_create(ui_scr_mode);
    lv_label_set_text(sec, "— 系统信息 —");
    lv_obj_set_style_text_color(sec, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(sec, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(sec, 10, ly); ly += 32;

    ui_label_info_fw = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_info_fw, "固件版本:  MY_OTA_GUI v1.0.0");
    lv_obj_set_style_text_color(ui_label_info_fw, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_fw, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_fw, 10, ly); ly += 28;

    ui_label_info_flash = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_info_flash, "Flash:  864KB APP");
    lv_obj_set_style_text_color(ui_label_info_flash, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_flash, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_flash, 10, ly); ly += 28;

    ui_label_info_sram = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_info_sram, "SRAM:  -- KB / 128 KB");
    lv_obj_set_style_text_color(ui_label_info_sram, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_sram, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_sram, 10, ly); ly += 28;

    ui_label_info_heap = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_info_heap, "堆剩余:  -- / 24576");
    lv_obj_set_style_text_color(ui_label_info_heap, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_heap, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_heap, 10, ly); ly += 28;

    ui_label_info_ota = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_info_ota, "OTA 完成:  0 次");
    lv_obj_set_style_text_color(ui_label_info_ota, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_ota, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_ota, 10, ly); ly += 28;

    ui_label_info_uptime = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_info_uptime, "运行时间:  00m00s");
    lv_obj_set_style_text_color(ui_label_info_uptime, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_uptime, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_uptime, 10, ly); ly += 32;

    ly += 8;
    sec = lv_label_create(ui_scr_mode);
    lv_label_set_text(sec, "— 连接状态 —");
    lv_obj_set_style_text_color(sec, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(sec, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(sec, 10, ly); ly += 32;

    ui_label_info_can = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_info_can, "CAN 总线:  等待数据...");
    lv_obj_set_style_text_color(ui_label_info_can, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_can, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_can, 10, ly); ly += 28;

    ui_label_info_f103 = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_info_f103, "F103 节点: 等待数据...");
    lv_obj_set_style_text_color(ui_label_info_f103, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_f103, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_f103, 10, ly); ly += 28;

    /* ═══════════════════════════════════════════════════════
     *  右侧：硬件诊断 (x=400, 竖中线)
     * ═══════════════════════════════════════════════════════ */
    int ry = 50;

    sec = lv_label_create(ui_scr_mode);
    lv_label_set_text(sec, "— 硬件诊断 —");
    lv_obj_set_style_text_color(sec, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(sec, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(sec, 400, ry); ry += 32;

    ui_label_diag_spi = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_diag_spi, "SPI Flash:  W25Q128  16 MB");
    lv_obj_set_style_text_color(ui_label_diag_spi, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_spi, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_spi, 400, ry); ry += 28;

    ui_label_diag_can = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_diag_can, "CAN:  500kbps  已初始化");
    lv_obj_set_style_text_color(ui_label_diag_can, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_can, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_can, 400, ry); ry += 28;

    ui_label_diag_lcd = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_diag_lcd, "LCD:  NT35510  800×480  16bit");
    lv_obj_set_style_text_color(ui_label_diag_lcd, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_lcd, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_lcd, 400, ry); ry += 28;

    ui_label_diag_touch = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_diag_touch, "触摸:  GT911  I2C 软件模拟");
    lv_obj_set_style_text_color(ui_label_diag_touch, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_touch, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_touch, 400, ry); ry += 28;

    ui_label_diag_uptime = lv_label_create(ui_scr_mode);
    lv_label_set_text(ui_label_diag_uptime, "系统运行:  00m00s");
    lv_obj_set_style_text_color(ui_label_diag_uptime, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_uptime, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_uptime, 400, ry); ry += 28;
}

/* ═════════════════════════════════════════════════════════════
 *  切换函数
 * ═════════════════════════════════════════════════════════════ */
void mode_ui_show(void)
{
    if (ui_scr_mode) lv_scr_load(ui_scr_mode);
}

void mode_ui_hide(void)
{
    extern lv_obj_t *ui_scr_main;
    if (ui_scr_main) lv_scr_load(ui_scr_main);
}

/* ═════════════════════════════════════════════════════════════
 *  信息页刷新（由 sysmon_timer_cb 定时调用）
 * ═════════════════════════════════════════════════════════════ */
void mode_ui_update_info(uint32_t heap_free, uint32_t heap_total,
                         uint32_t uptime_sec, uint16_t ota_count,
                         uint8_t can_online)
{
    char buf[64];

    uint32_t used = heap_total - heap_free;
    uint32_t pct = used * 100 / heap_total;
    snprintf(buf, sizeof(buf), "堆剩余:  %lu / %lu  (%lu%%)", used, heap_total, pct);
    lv_label_set_text(ui_label_info_heap, buf);

    uint32_t min = uptime_sec / 60;
    uint32_t sec = uptime_sec % 60;
    snprintf(buf, sizeof(buf), "运行时间:  %02lum%02lus", min, sec);
    lv_label_set_text(ui_label_info_uptime, buf);
    snprintf(buf, sizeof(buf), "系统运行:  %02lum%02lus", min, sec);
    lv_label_set_text(ui_label_diag_uptime, buf);

    snprintf(buf, sizeof(buf), "OTA 完成:  %u 次", ota_count);
    lv_label_set_text(ui_label_info_ota, buf);

    lv_label_set_text(ui_label_info_can,
        can_online ? "CAN 总线:  已连接" : "CAN 总线:  未连接");
    lv_obj_set_style_text_color(ui_label_info_can,
        can_online ? lv_color_hex(0x44ff44) : lv_color_hex(0xff4444), LV_PART_MAIN);

    /* SRAM 占用估算 */
    uint32_t sram_free = (uint32_t)xPortGetFreeHeapSize();
    uint32_t sram_total = 128 * 1024;
    snprintf(buf, sizeof(buf), "SRAM:  %lu KB / 128 KB", (sram_total - sram_free) / 1024);
    lv_label_set_text(ui_label_info_sram, buf);
}

/* ═════════════════════════════════════════════════════════════
 *  主题切换
 * ═════════════════════════════════════════════════════════════ */
void mode_ui_apply_theme(uint8_t tid)
{
    #define _C(r,g,b) LV_COLOR_MAKE(r,g,b)
    static const lv_color_t bg[3] = {
        _C(0,0,0), _C(0x44,0x44,0x44), _C(0x1a,0x2e,0x1a)};
    static const lv_color_t bar[3] = {
        _C(0x11,0x11,0x11), _C(0x33,0x33,0x33), _C(0x1a,0x30,0x1a)};

    if (tid > 2) return;
    lv_obj_set_style_bg_color(ui_scr_mode, bg[tid], LV_PART_MAIN | LV_STATE_DEFAULT);
    /* 遍历顶栏及其父背景（top_bar 是 ui_scr_mode 的第一个子对象） */
    lv_obj_t *child = lv_obj_get_child(ui_scr_mode, 0);
    if (child) lv_obj_set_style_bg_color(child, bar[tid], LV_PART_MAIN);
}

/* ═════════════════════════════════════════════════════════════
 *  事件回调
 * ═════════════════════════════════════════════════════════════ */
static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    mode_ui_hide();
}
