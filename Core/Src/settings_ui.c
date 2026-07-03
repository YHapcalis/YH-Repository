/**
 * @file    settings_ui.c
 * @brief   设置页 — 系统操作（重启/Bootloader/恢复出厂）+ 主题切换
 */

#include "settings_ui.h"
#include "app_ui.h"
#include "inter_flash_cfg.h"
#include "lvgl.h"
#include "FreeRTOS.h"
#include "task.h"    /* osDelay */
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"   /* NVIC_SystemReset */
#include <stdio.h>

/* ── 自定义中文字体 ── */
LV_FONT_DECLARE(ui_font_chinese_16);

/* ── 控件句柄 ── */
static lv_obj_t *ui_scr_settings;
static lv_obj_t *ui_settings_topbar;

/* 主题按钮 */
static lv_obj_t *btn_theme[3];

/* ── 前置声明 ── */
static void btn_back_cb(lv_event_t *e);
static void btn_reboot_cb(lv_event_t *e);
static void btn_bootloader_cb(lv_event_t *e);
static void btn_factory_reset_cb(lv_event_t *e);
static void btn_theme_cb(lv_event_t *e);
static void update_theme_buttons(void);

/* ═════════════════════════════════════════════════════════════
 *  创建设置屏
 * ═════════════════════════════════════════════════════════════ */
void settings_ui_create(void)
{
    uint8_t cur = ui_get_theme();

    ui_scr_settings = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scr_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scr_settings, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_scr_settings, 255, LV_PART_MAIN);

    /* ── 顶栏 ── */
    ui_settings_topbar = lv_obj_create(ui_scr_settings);
    lv_obj_set_size(ui_settings_topbar, 800, 32);
    lv_obj_set_pos(ui_settings_topbar, 0, 0);
    lv_obj_clear_flag(ui_settings_topbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_settings_topbar, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_settings_topbar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_settings_topbar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui_settings_topbar, 0, LV_PART_MAIN);

    /* ← 返回 */
    lv_obj_t *btn_back = lv_btn_create(ui_settings_topbar);
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

    lv_obj_t *lbl_title = lv_label_create(ui_settings_topbar);
    lv_label_set_text(lbl_title, "设置");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    /* ═══════════════════════════════════════════════════════
     *  系统操作
     * ═══════════════════════════════════════════════════════ */
    int y = 50;

    lv_obj_t *sec = lv_label_create(ui_scr_settings);
    lv_label_set_text(sec, "— 系统操作 —");
    lv_obj_set_style_text_color(sec, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(sec, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(sec, 10, y); y += 36;

    /* 重启系统 */
    lv_obj_t *btn = lv_btn_create(ui_scr_settings);
    lv_obj_set_size(btn, 240, 36);
    lv_obj_set_pos(btn, 10, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x881111), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, btn_reboot_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "重启系统");
    lv_obj_set_style_text_font(lbl, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_center(lbl);
    y += 44;

    /* 进入 Bootloader */
    btn = lv_btn_create(ui_scr_settings);
    lv_obj_set_size(btn, 240, 36);
    lv_obj_set_pos(btn, 10, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x884411), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, btn_bootloader_cb, LV_EVENT_CLICKED, NULL);
    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "进入 Bootloader");
    lv_obj_set_style_text_font(lbl, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_center(lbl);
    y += 44;

    /* 恢复出厂设置 */
    btn = lv_btn_create(ui_scr_settings);
    lv_obj_set_size(btn, 240, 36);
    lv_obj_set_pos(btn, 10, y);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, btn_factory_reset_cb, LV_EVENT_CLICKED, NULL);
    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "恢复出厂设置");
    lv_obj_set_style_text_font(lbl, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_center(lbl);
    y += 44;

    /* ═══════════════════════════════════════════════════════
     *  主题选择
     * ═══════════════════════════════════════════════════════ */
    y += 12;
    sec = lv_label_create(ui_scr_settings);
    lv_label_set_text(sec, "— 主题 —");
    lv_obj_set_style_text_color(sec, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(sec, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(sec, 10, y); y += 36;

    static const char *theme_names[] = {"暗黑", "明亮", "护眼"};
    for (int i = 0; i < 3; i++) {
        btn_theme[i] = lv_btn_create(ui_scr_settings);
        lv_obj_set_size(btn_theme[i], 100, 34);
        lv_obj_set_pos(btn_theme[i], 10 + i * 110, y);
        lv_obj_set_style_radius(btn_theme[i], 6, LV_PART_MAIN);
        lv_obj_add_event_cb(btn_theme[i], btn_theme_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
        lbl = lv_label_create(btn_theme[i]);
        lv_label_set_text(lbl, theme_names[i]);
        lv_obj_set_style_text_font(lbl, &ui_font_chinese_16, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), LV_PART_MAIN);
        lv_obj_center(lbl);
    }
    update_theme_buttons();

    /* 应用当前主题 */
    settings_ui_apply_theme(cur);
}

/* ═════════════════════════════════════════════════════════════
 *  切换函数
 * ═════════════════════════════════════════════════════════════ */
void settings_ui_show(void)
{
    if (ui_scr_settings) {
        update_theme_buttons();
        lv_scr_load(ui_scr_settings);
    }
}

void settings_ui_hide(void)
{
    extern lv_obj_t *ui_scr_main;
    if (ui_scr_main) lv_scr_load(ui_scr_main);
}

/* ═════════════════════════════════════════════════════════════
 *  主题切换接口
 * ═════════════════════════════════════════════════════════════ */
void settings_ui_apply_theme(uint8_t tid)
{
    #define _C(r,g,b) LV_COLOR_MAKE(r,g,b)
    static const lv_color_t bg[3] = {
        _C(0,0,0), _C(0x44,0x44,0x44), _C(0x1a,0x2e,0x1a)};
    static const lv_color_t bar[3] = {
        _C(0x11,0x11,0x11), _C(0x33,0x33,0x33), _C(0x1a,0x30,0x1a)};

    if (tid > 2) return;
    if (!ui_scr_settings) return;
    lv_obj_set_style_bg_color(ui_scr_settings, bg[tid], LV_PART_MAIN | LV_STATE_DEFAULT);
    if (ui_settings_topbar)
        lv_obj_set_style_bg_color(ui_settings_topbar, bar[tid], LV_PART_MAIN);
}

/* ═════════════════════════════════════════════════════════════
 *  事件回调
 * ═════════════════════════════════════════════════════════════ */
static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_hide();
}

static void btn_reboot_cb(lv_event_t *e)
{
    (void)e;
    printf("[Settings] Reboot...\r\n");
    osDelay(100);
    NVIC_SystemReset();
}

static void btn_bootloader_cb(lv_event_t *e)
{
    (void)e;
    printf("[Settings] OTA mode...\r\n");
    inter_flash_cfg_set_app_update_flag(1);
    osDelay(100);
    NVIC_SystemReset();
}

static void btn_factory_reset_cb(lv_event_t *e)
{
    (void)e;
    printf("[Settings] Factory reset...\r\n");
    /* 清 OTA 计数 */
    uint8_t zero[2] = {0, 0};
    inter_flash_cfg_inc_ota_count();   /* 先增到 1 再复位清零简化 */
    inter_flash_cfg_set_app_update_flag(0);
    printf("[Settings] OTA count cleared. Reboot...\r\n");
    osDelay(100);
    NVIC_SystemReset();
}

static void btn_theme_cb(lv_event_t *e)
{
    uint8_t tid = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (tid > THEME_GREEN) return;
    ui_apply_theme(tid);
    update_theme_buttons();
}

static void update_theme_buttons(void)
{
    uint8_t cur = ui_get_theme();
    #define _C2(r,g,b) LV_COLOR_MAKE(r,g,b)
    static const lv_color_t active_bg   = _C2(0x33,0x66,0x33);
    static const lv_color_t inactive_bg = _C2(0x44,0x44,0x44);
    for (int i = 0; i < 3; i++) {
        if (!btn_theme[i]) continue;
        lv_obj_set_style_bg_color(btn_theme[i],
            (i == cur) ? active_bg : inactive_bg, LV_PART_MAIN);
    }
}
