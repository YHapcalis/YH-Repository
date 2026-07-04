/**
 * @file    mode_ui.c
 * @brief   Mode 屏 — 左侧信息 + 右侧诊断，卡片 + 指示圆点
 */

#include "mode_ui.h"
#include "lvgl.h"
#include "en25q128.h"
#include "FreeRTOS.h"
#include "task.h"
#include "spi_img_loader.h"
#include <stdio.h>

LV_FONT_DECLARE(ui_font_chinese_16);

/* ── 控件句柄 ── */
static lv_obj_t *ui_scr_mode;
static lv_obj_t *ui_card_left;
static lv_obj_t *ui_card_right;

/* 左侧信息 */
static lv_obj_t *ui_label_info_flash;
static lv_obj_t *ui_label_info_sram;
static lv_obj_t *ui_label_info_heap;
static lv_obj_t *ui_label_info_ota;
static lv_obj_t *ui_label_info_uptime;
static lv_obj_t *ui_label_info_fw;
static lv_obj_t *ui_label_info_can;
static lv_obj_t *ui_label_info_f103;
static lv_obj_t *ui_led_can;
static lv_obj_t *ui_led_f103;

/* 右侧诊断 */
static lv_obj_t *ui_label_diag_spi;
static lv_obj_t *ui_label_diag_can;
static lv_obj_t *ui_label_diag_lcd;
static lv_obj_t *ui_label_diag_touch;
static lv_obj_t *ui_label_diag_uptime;
static lv_obj_t *ui_label_diag_can_err;
static lv_obj_t *ui_label_diag_reset;
static lv_obj_t *ui_label_diag_sram;

/* ── 前置声明 ── */
static void btn_back_cb(lv_event_t *e);
static void set_led_color(lv_obj_t *led, uint8_t on);

/* ═════════════════════════════════════════════════════════════
 *  创建 Mode 屏
 * ═════════════════════════════════════════════════════════════ */
void mode_ui_create(void)
{
    ui_scr_mode = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scr_mode, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scr_mode, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_scr_mode, 255, LV_PART_MAIN);

    /* ── 顶栏 ── */
    lv_obj_t *top_bar = lv_obj_create(ui_scr_mode);
    lv_obj_set_size(top_bar, 800, 32);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_border_width(top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(top_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(top_bar, 0, LV_PART_MAIN);

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

    lv_obj_t *lbl_title = lv_label_create(top_bar);
    lv_label_set_text(lbl_title, "Mode");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    /* ── 中间竖分隔线 ── */
    lv_obj_t *vline = lv_obj_create(ui_scr_mode);
    lv_obj_set_size(vline, 1, 420);
    lv_obj_set_pos(vline, 399, 40);
    lv_obj_clear_flag(vline, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(vline, lv_color_hex(0x334466), LV_PART_MAIN);
    lv_obj_set_style_border_width(vline, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(vline, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(vline, 80, LV_PART_MAIN);

    /* ═══════════════════════════════════════════════════════
     *  左侧卡片：系统信息 + 连接状态
     * ═══════════════════════════════════════════════════════ */
    ui_card_left = lv_obj_create(ui_scr_mode);
    lv_obj_set_size(ui_card_left, 380, 430);
    lv_obj_set_pos(ui_card_left, 8, 40);
    lv_obj_clear_flag(ui_card_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_card_left, lv_color_hex(0x0d0d1a), LV_PART_MAIN);
    lv_obj_set_style_border_color(ui_card_left, lv_color_hex(0x334466), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_card_left, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_card_left, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui_card_left, 10, LV_PART_MAIN);

    int ly = 0;
    lv_obj_t *sec;

    sec = lv_label_create(ui_card_left);
    lv_label_set_text(sec, "— 系统信息 —");
    lv_obj_set_style_text_color(sec, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(sec, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(sec, 0, ly); ly += 32;

    ui_label_info_fw = lv_label_create(ui_card_left);
    lv_label_set_text(ui_label_info_fw, "固件版本:  MY_OTA_GUI v1.0.0");
    lv_obj_set_style_text_color(ui_label_info_fw, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_fw, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_fw, 0, ly); ly += 26;

    ui_label_info_flash = lv_label_create(ui_card_left);
    lv_label_set_text(ui_label_info_flash, "Flash:  864KB APP");
    lv_obj_set_style_text_color(ui_label_info_flash, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_flash, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_flash, 0, ly); ly += 26;

    ui_label_info_sram = lv_label_create(ui_card_left);
    lv_label_set_text(ui_label_info_sram, "SRAM:  -- KB / 128 KB");
    lv_obj_set_style_text_color(ui_label_info_sram, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_sram, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_sram, 0, ly); ly += 26;

    ui_label_info_heap = lv_label_create(ui_card_left);
    lv_label_set_text(ui_label_info_heap, "堆剩余:  -- / 24576");
    lv_obj_set_style_text_color(ui_label_info_heap, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_heap, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_heap, 0, ly); ly += 26;

    ui_label_info_ota = lv_label_create(ui_card_left);
    lv_label_set_text(ui_label_info_ota, "OTA 完成:  0 次");
    lv_obj_set_style_text_font(ui_label_info_ota, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_ota, 0, ly); ly += 26;

    ui_label_info_uptime = lv_label_create(ui_card_left);
    lv_label_set_text(ui_label_info_uptime, "运行时间:  00m00s");
    lv_obj_set_style_text_color(ui_label_info_uptime, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_uptime, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_uptime, 0, ly); ly += 30;

    ly += 4;
    sec = lv_label_create(ui_card_left);
    lv_label_set_text(sec, "— 连接状态 —");
    lv_obj_set_style_text_color(sec, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(sec, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(sec, 0, ly); ly += 30;

    /* CAN 行：LED 圆点 + 标签 */
    ui_led_can = lv_led_create(ui_card_left);
    lv_obj_set_size(ui_led_can, 12, 12);
    lv_obj_set_pos(ui_led_can, 0, ly + 4);
    set_led_color(ui_led_can, 0);

    ui_label_info_can = lv_label_create(ui_card_left);
    lv_label_set_text(ui_label_info_can, "CAN 总线:  等待数据...");
    lv_obj_set_style_text_color(ui_label_info_can, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_can, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_can, 18, ly); ly += 26;

    /* F103 行：LED 圆点 + 标签 */
    ui_led_f103 = lv_led_create(ui_card_left);
    lv_obj_set_size(ui_led_f103, 12, 12);
    lv_obj_set_pos(ui_led_f103, 0, ly + 4);
    set_led_color(ui_led_f103, 0);

    ui_label_info_f103 = lv_label_create(ui_card_left);
    lv_label_set_text(ui_label_info_f103, "F103 节点: 等待数据...");
    lv_obj_set_style_text_color(ui_label_info_f103, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_info_f103, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_info_f103, 18, ly); ly += 26;

    /* ═══════════════════════════════════════════════════════
     *  右侧卡片：硬件诊断
     * ═══════════════════════════════════════════════════════ */
    ui_card_right = lv_obj_create(ui_scr_mode);
    lv_obj_set_size(ui_card_right, 380, 430);
    lv_obj_set_pos(ui_card_right, 410, 40);
    lv_obj_clear_flag(ui_card_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_card_right, lv_color_hex(0x0d0d1a), LV_PART_MAIN);
    lv_obj_set_style_border_color(ui_card_right, lv_color_hex(0x334466), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_card_right, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_card_right, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui_card_right, 10, LV_PART_MAIN);

    int ry = 0;
    sec = lv_label_create(ui_card_right);
    lv_label_set_text(sec, "— 硬件诊断 —");
    lv_obj_set_style_text_color(sec, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(sec, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(sec, 0, ry); ry += 32;

    ui_label_diag_spi = lv_label_create(ui_card_right);
    lv_label_set_text(ui_label_diag_spi, "SPI Flash:  W25Q128  16 MB");
    lv_obj_set_style_text_color(ui_label_diag_spi, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_spi, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_spi, 0, ry); ry += 26;

    ui_label_diag_can = lv_label_create(ui_card_right);
    lv_label_set_text(ui_label_diag_can, "CAN:  500kbps  已初始化");
    lv_obj_set_style_text_color(ui_label_diag_can, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_can, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_can, 0, ry); ry += 26;

    ui_label_diag_lcd = lv_label_create(ui_card_right);
    lv_label_set_text(ui_label_diag_lcd, "LCD:  NT35510  800×480  16bit");
    lv_obj_set_style_text_color(ui_label_diag_lcd, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_lcd, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_lcd, 0, ry); ry += 26;

    ui_label_diag_touch = lv_label_create(ui_card_right);
    lv_label_set_text(ui_label_diag_touch, "触摸:  GT911  I2C 软件模拟");
    lv_obj_set_style_text_color(ui_label_diag_touch, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_touch, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_touch, 0, ry); ry += 26;

    ui_label_diag_uptime = lv_label_create(ui_card_right);
    lv_label_set_text(ui_label_diag_uptime, "系统运行:  00m00s");
    lv_obj_set_style_text_color(ui_label_diag_uptime, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_uptime, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_uptime, 0, ry); ry += 26;

    /* CAN 错误计数器 */
    ui_label_diag_can_err = lv_label_create(ui_card_right);
    lv_label_set_text(ui_label_diag_can_err, "CAN 错误:  无");
    lv_obj_set_style_text_color(ui_label_diag_can_err, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_can_err, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_can_err, 0, ry); ry += 26;

    /* 上次复位原因（启动时读一次即锁存） */
    uint32_t csr = RCC->CSR;
    const char *reset_src;
    if (csr & RCC_CSR_PORRSTF)      reset_src = "上电复位";
    else if (csr & RCC_CSR_SFTRSTF) reset_src = "软件复位";
    else if (csr & RCC_CSR_PINRSTF) reset_src = "外部复位";
    else if (csr & RCC_CSR_IWDGRSTF) reset_src = "看门狗复位";
    else                            reset_src = "未知";
    RCC->CSR |= RCC_CSR_RMVF;  /* 清除标志 */

    ui_label_diag_reset = lv_label_create(ui_card_right);
    char rbuf[48];
    snprintf(rbuf, sizeof(rbuf), "上次复位:  %s", reset_src);
    lv_label_set_text(ui_label_diag_reset, rbuf);
    lv_obj_set_style_text_color(ui_label_diag_reset, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_reset, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_reset, 0, ry); ry += 26;

    /* 外部 SRAM */
    ui_label_diag_sram = lv_label_create(ui_card_right);
    lv_label_set_text(ui_label_diag_sram, "外部 SRAM:  待检测");
    lv_obj_set_style_text_color(ui_label_diag_sram, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_diag_sram, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(ui_label_diag_sram, 0, ry); ry += 26;
}

/* ═════════════════════════════════════════════════════════════
 *  切换函数
 * ═════════════════════════════════════════════════════════════ */
void mode_ui_show(void)
{
    if (ui_scr_mode) lv_scr_load_anim(ui_scr_mode, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

void mode_ui_hide(void)
{
    extern lv_obj_t *ui_scr_main;
    if (ui_scr_main) lv_scr_load_anim(ui_scr_main, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

/* ═════════════════════════════════════════════════════════════
 *  信息页刷新
 * ═════════════════════════════════════════════════════════════ */
void mode_ui_update_info(uint32_t heap_free, uint32_t heap_total,
                         uint32_t uptime_sec, uint16_t ota_count,
                         uint8_t can_online)
{
    char buf[64];

    /* ── 堆剩余 + 着色 ── */
    uint32_t used = heap_total - heap_free;
    uint32_t pct = used * 100 / heap_total;
    lv_color_t heap_col;
    if (pct < 30)      heap_col = lv_color_hex(0x44ff44);   /* 绿 */
    else if (pct < 60) heap_col = lv_color_hex(0xffaa00);   /* 黄 */
    else               heap_col = lv_color_hex(0xff4444);   /* 红 */
    snprintf(buf, sizeof(buf), "堆剩余:  %lu / %lu (%lu%%)", used, heap_total, pct);
    lv_label_set_text(ui_label_info_heap, buf);
    lv_obj_set_style_text_color(ui_label_info_heap, heap_col, LV_PART_MAIN);

    /* ── 运行时间 ── */
    uint32_t min = uptime_sec / 60;
    uint32_t sec = uptime_sec % 60;
    snprintf(buf, sizeof(buf), "运行时间:  %02lum%02lus", min, sec);
    lv_label_set_text(ui_label_info_uptime, buf);
    snprintf(buf, sizeof(buf), "系统运行:  %02lum%02lus", min, sec);
    lv_label_set_text(ui_label_diag_uptime, buf);

    /* ── OTA 完成 + 着色 ── */
    lv_color_t ota_col = (ota_count > 0) ? lv_color_hex(0x44ff44) : lv_color_hex(0xaaaaaa);
    snprintf(buf, sizeof(buf), "OTA 完成:  %u 次", ota_count);
    lv_label_set_text(ui_label_info_ota, buf);
    lv_obj_set_style_text_color(ui_label_info_ota, ota_col, LV_PART_MAIN);

    /* ── CAN + F103 状态 + LED ── */
    lv_label_set_text(ui_label_info_can,
        can_online ? "CAN 总线:  已连接" : "CAN 总线:  未连接");
    lv_obj_set_style_text_color(ui_label_info_can,
        can_online ? lv_color_hex(0x44ff44) : lv_color_hex(0xff4444), LV_PART_MAIN);
    set_led_color(ui_led_can, can_online);

    /* F103 在线状态跟随 CAN（当前通过同一传感器帧判断） */
    lv_label_set_text(ui_label_info_f103,
        can_online ? "F103 节点:  活跃" : "F103 节点:  超时");
    lv_obj_set_style_text_color(ui_label_info_f103,
        can_online ? lv_color_hex(0x44ff44) : lv_color_hex(0xff4444), LV_PART_MAIN);
    set_led_color(ui_led_f103, can_online);

    /* ── FreeRTOS 堆占用 ── */
    uint32_t heap_free_now = (uint32_t)xPortGetFreeHeapSize();
    uint32_t heap_used = heap_total - heap_free_now;
    uint32_t heap_pct = heap_used * 100 / heap_total;
    snprintf(buf, sizeof(buf), "FreeRTOS 堆:  %lu%%  (%lu/%lu)", heap_pct, heap_used, heap_total);
    lv_label_set_text(ui_label_info_sram, buf);

    /* ── 外部 SRAM + 图片数据源 ── */
    {
        const char *src_str = spi_img_get_source() ?
            "外部 SRAM:  IS62WV51216  (图片@SRAM)" :
            "外部 SRAM:  IS62WV51216  (图片@Flash)";
        lv_label_set_text(ui_label_diag_sram, src_str);
    }

    /* ── CAN 错误计数器 ── */
    uint32_t esr = CAN1->ESR;
    uint8_t rec = (esr >> 16) & 0xFF;
    uint8_t tec = (esr >> 8) & 0xFF;
    const char *can_state;
    if (esr & (1 << 2))       can_state = "总线关闭";
    else if (esr & (1 << 1))  can_state = "被动错误";
    else if (esr & 1)         can_state = "主动错误";
    else                      can_state = "无错误";
    snprintf(buf, sizeof(buf), "CAN 错误:  %s  (%d/%d)", can_state, rec, tec);
    lv_label_set_text(ui_label_diag_can_err, buf);
    lv_obj_set_style_text_color(ui_label_diag_can_err,
        (esr & 0x07) ? lv_color_hex(0xffaa00) : lv_color_hex(0x44ff44), LV_PART_MAIN);
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
    static const lv_color_t card[3] = {
        _C(0x0d,0x0d,0x1a), _C(0x44,0x44,0x55), _C(0x20,0x30,0x20)};

    if (tid > 2) return;
    lv_obj_set_style_bg_color(ui_scr_mode, bg[tid], LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *child = lv_obj_get_child(ui_scr_mode, 0);
    if (child) lv_obj_set_style_bg_color(child, bar[tid], LV_PART_MAIN);
    if (ui_card_left) lv_obj_set_style_bg_color(ui_card_left, card[tid], LV_PART_MAIN);
    if (ui_card_right) lv_obj_set_style_bg_color(ui_card_right, card[tid], LV_PART_MAIN);
}

/* ═════════════════════════════════════════════════════════════
 *  内部工具
 * ═════════════════════════════════════════════════════════════ */
static void set_led_color(lv_obj_t *led, uint8_t on)
{
    if (on) {
        lv_led_set_color(led, lv_color_hex(0x44ff44));
        lv_led_on(led);
    } else {
        lv_led_set_color(led, lv_color_hex(0xff4444));
        lv_led_off(led);
    }
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    mode_ui_hide();
}
