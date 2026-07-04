/**
 * @file    clock_ui.c
 * @brief   日期时钟页 — 左日历 + 右滚轮时间 + 同步到 F103 RTC
 *
 * 通过 CAN ID 0x14 发送 [year][month][day][hour][min][sec][sum] 到 F103
 */

#include "clock_ui.h"
#include "lvgl.h"
#include "canif.h"
#include <stdio.h>

LV_FONT_DECLARE(ui_font_chinese_16);

/* 滚轮选项 */
#define HOURS_STR "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23"
#define MINS_STR  "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59"

/* ── 控件句柄 ── */
static lv_obj_t *ui_scr_clock;
static lv_obj_t *ui_calendar;
static lv_obj_t *ui_roller_hour;
static lv_obj_t *ui_roller_min;
static lv_obj_t *ui_label_cur_time;
static lv_obj_t *ui_label_sel_time;
static lv_obj_t *ui_label_status;

/* 当前时间（动态，来自 F103 CAN 帧，仅供显示） */
static uint8_t s_cur_year, s_cur_month, s_cur_day;
static uint8_t s_cur_hour, s_cur_min, s_cur_sec;

/* 选定时间（静态，由日历 + 滚轮设定，确认时发送） */
static uint8_t s_sel_year = 26, s_sel_month = 7, s_sel_day = 3;
static uint8_t s_sel_hour = 0,  s_sel_min = 0,  s_sel_sec = 0;

/* 日历高亮 */
static lv_calendar_date_t s_highlight;

/* ── 前置声明 ── */
static void btn_back_cb(lv_event_t *e);
static void btn_confirm_cb(lv_event_t *e);
static void calendar_event_cb(lv_event_t *e);
static void roller_event_cb(lv_event_t *e);
static void update_selected_label(void);

/* ═════════════════════════════════════════════════════════════
 *  创建日期时钟屏
 * ═════════════════════════════════════════════════════════════ */
void clock_ui_create(void)
{
    ui_scr_clock = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scr_clock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scr_clock, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_scr_clock, 255, LV_PART_MAIN);

    /* ── 顶栏 ── */
    lv_obj_t *top_bar = lv_obj_create(ui_scr_clock);
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
    lv_label_set_text(lbl_title, "日期时钟");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    /* ── 动态当前时间（顶部居中）── */
    ui_label_cur_time = lv_label_create(ui_scr_clock);
    lv_label_set_text(ui_label_cur_time, "当前时间:  2026-07-03  00:00:00");
    lv_obj_set_style_text_color(ui_label_cur_time, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_cur_time, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_cur_time, LV_ALIGN_CENTER, 0, -185);  /* 240-185=55 */

    /* ═══════════════════════════════════════════════════════
     *  左卡片：日历
     * ═══════════════════════════════════════════════════════ */
    lv_obj_t *card_left = lv_obj_create(ui_scr_clock);
    lv_obj_set_size(card_left, 375, 285);
    lv_obj_set_pos(card_left, 15, 85);
    lv_obj_clear_flag(card_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card_left, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_color(card_left, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_border_width(card_left, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card_left, 8, LV_PART_MAIN);

    ui_calendar = lv_calendar_create(card_left);
    lv_obj_set_size(ui_calendar, 340, 260);
    lv_obj_center(ui_calendar);
    lv_calendar_set_showed_date(ui_calendar, 2026, 7);
    lv_calendar_header_arrow_create(ui_calendar);
    lv_obj_add_event_cb(ui_calendar, calendar_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ═══════════════════════════════════════════════════════
     *  右卡片：滚轮时间选择
     * ═══════════════════════════════════════════════════════ */
    lv_obj_t *card_right = lv_obj_create(ui_scr_clock);
    lv_obj_set_size(card_right, 375, 285);
    lv_obj_set_pos(card_right, 410, 85);
    lv_obj_clear_flag(card_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card_right, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_border_color(card_right, lv_color_hex(0xcccccc), LV_PART_MAIN);
    lv_obj_set_style_border_width(card_right, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card_right, 8, LV_PART_MAIN);

    /* 小时滚轮 — 白底黑字 */
    ui_roller_hour = lv_roller_create(card_right);
    lv_roller_set_options(ui_roller_hour, HOURS_STR, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(ui_roller_hour, 90, 170);
    lv_obj_set_style_text_align(ui_roller_hour, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ui_roller_hour, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_roller_hour, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_roller_hour, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_roller_hour, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(ui_roller_hour, 20, LV_PART_MAIN);
    lv_obj_set_pos(ui_roller_hour, 55, 55);
    lv_obj_add_event_cb(ui_roller_hour, roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 冒号分隔 — 深灰 */
    lv_obj_t *lb_colon = lv_label_create(card_right);
    lv_label_set_text(lb_colon, ":");
    lv_obj_set_style_text_color(lb_colon, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_text_font(lb_colon, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_pos(lb_colon, 172, 115);

    /* 分钟滚轮 — 白底黑字 */
    ui_roller_min = lv_roller_create(card_right);
    lv_roller_set_options(ui_roller_min, MINS_STR, LV_ROLLER_MODE_NORMAL);
    lv_obj_set_size(ui_roller_min, 90, 170);
    lv_obj_set_style_text_align(ui_roller_min, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ui_roller_min, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_roller_min, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_roller_min, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_roller_min, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_top(ui_roller_min, 20, LV_PART_MAIN);
    lv_obj_set_pos(ui_roller_min, 220, 55);
    lv_obj_add_event_cb(ui_roller_min, roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 时/分 标签 */
    lv_obj_t *lb_hint_h = lv_label_create(card_right);
    lv_label_set_text(lb_hint_h, "时");
    lv_obj_set_style_text_color(lb_hint_h, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(lb_hint_h, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(lb_hint_h, 97, 230);

    lv_obj_t *lb_hint_m = lv_label_create(card_right);
    lv_label_set_text(lb_hint_m, "分");
    lv_obj_set_style_text_color(lb_hint_m, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_text_font(lb_hint_m, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_pos(lb_hint_m, 262, 230);

    /* ── 选定时间标签（下方居中）── */
    ui_label_sel_time = lv_label_create(ui_scr_clock);
    lv_label_set_text(ui_label_sel_time, "选定:  2026-07-03  00:00");
    lv_obj_set_style_text_color(ui_label_sel_time, lv_color_hex(0x44ff44), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_sel_time, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_sel_time, LV_ALIGN_CENTER, 0, 160);

    /* ── 确认按钮 ── */
    lv_obj_t *btn_cfm = lv_btn_create(ui_scr_clock);
    lv_obj_set_size(btn_cfm, 200, 36);
    lv_obj_align(btn_cfm, LV_ALIGN_CENTER, 0, 195);
    lv_obj_set_style_bg_color(btn_cfm, lv_color_hex(0x336633), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_cfm, 6, LV_PART_MAIN);
    lv_obj_add_event_cb(btn_cfm, btn_confirm_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_cfm = lv_label_create(btn_cfm);
    lv_label_set_text(lbl_cfm, "确认设置");
    lv_obj_set_style_text_font(lbl_cfm, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_cfm, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_center(lbl_cfm);

    /* 状态提示 */
    ui_label_status = lv_label_create(ui_scr_clock);
    lv_label_set_text(ui_label_status, "");
    lv_obj_set_style_text_color(ui_label_status, lv_color_hex(0x44ff44), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_status, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_status, LV_ALIGN_CENTER, 0, 225);
}

/* ═════════════════════════════════════════════════════════════
 *  切换函数
 * ═════════════════════════════════════════════════════════════ */
void clock_ui_show(void)
{
    if (ui_scr_clock) {
        lv_scr_load_anim(ui_scr_clock, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
        clock_ui_set_time(s_cur_year, s_cur_month, s_cur_day,
                          s_cur_hour, s_cur_min, s_cur_sec);
        /* 同步滚轮到选定值 */
        lv_roller_set_selected(ui_roller_hour, s_sel_hour, LV_ANIM_OFF);
        lv_roller_set_selected(ui_roller_min,  s_sel_min,  LV_ANIM_OFF);
        /* 日历高亮 */
        s_highlight.year  = 2000 + s_sel_year;
        s_highlight.month = s_sel_month;
        s_highlight.day   = s_sel_day;
        lv_calendar_set_highlighted_dates(ui_calendar, &s_highlight, 1);
        lv_calendar_set_showed_date(ui_calendar, s_highlight.year, s_highlight.month);
        update_selected_label();
        lv_label_set_text(ui_label_status, "");
    }
}

void clock_ui_hide(void)
{
    extern lv_obj_t *ui_scr_main;
    if (ui_scr_main) lv_scr_load_anim(ui_scr_main, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}

/* ═════════════════════════════════════════════════════════════
 *  时间更新接口
 * ═════════════════════════════════════════════════════════════ */
void clock_ui_set_time(uint8_t year, uint8_t month, uint8_t day,
                        uint8_t hour, uint8_t min, uint8_t sec)
{
    s_cur_year  = year;  s_cur_month = month;  s_cur_day  = day;
    s_cur_hour  = hour;  s_cur_min   = min;    s_cur_sec  = sec;
    char buf[48];
    snprintf(buf, sizeof(buf), "当前:  20%02u-%02u-%02u  %02u:%02u:%02u",
             year, month, day, hour, min, sec);
    if (ui_label_cur_time) lv_label_set_text(ui_label_cur_time, buf);
}

/* ═════════════════════════════════════════════════════════════
 *  事件回调
 * ═════════════════════════════════════════════════════════════ */
static void update_selected_label(void)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "选定:  20%02u-%02u-%02u  %02u:%02u",
             s_sel_year, s_sel_month, s_sel_day, s_sel_hour, s_sel_min);
    lv_label_set_text(ui_label_sel_time, buf);
}

static void calendar_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_calendar_date_t date;
        if (lv_calendar_get_pressed_date(ui_calendar, &date)) {
            s_highlight.year  = date.year;
            s_highlight.month = date.month;
            s_highlight.day   = date.day;
            lv_calendar_set_highlighted_dates(ui_calendar, &s_highlight, 1);
            s_sel_year  = date.year % 100;
            s_sel_month = date.month;
            s_sel_day   = date.day;
            s_sel_hour  = lv_roller_get_selected(ui_roller_hour);
            s_sel_min   = lv_roller_get_selected(ui_roller_min);
            s_sel_sec   = s_cur_sec;
            update_selected_label();
        }
    }
}

static void roller_event_cb(lv_event_t *e)
{
    (void)e;
    s_sel_hour = lv_roller_get_selected(ui_roller_hour);
    s_sel_min  = lv_roller_get_selected(ui_roller_min);
    update_selected_label();
}

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    clock_ui_hide();
}

static void btn_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* 同步滚轮的当前值到选定时间 */
    s_sel_hour = lv_roller_get_selected(ui_roller_hour);
    s_sel_min  = lv_roller_get_selected(ui_roller_min);

    uint8_t can_data[7];
    can_data[0] = s_sel_year;
    can_data[1] = s_sel_month;
    can_data[2] = s_sel_day;
    can_data[3] = s_sel_hour;
    can_data[4] = s_sel_min;
    can_data[5] = s_sel_sec;
    uint8_t sum = 0;
    for (int i = 0; i < 6; i++) sum += can_data[i];
    can_data[6] = sum;

    uint8_t ret = CAN1_Send_Msg(0x14, can_data, 7);
    printf("[CLOCK] Sent 0x14: 20%02u-%02u-%02u %02u:%02u sum=0x%02X ret=%u\r\n",
           s_sel_year, s_sel_month, s_sel_day, s_sel_hour, s_sel_min, sum, ret);

    if (ret == 0) {
        lv_label_set_text(ui_label_status, "时间已同步!");
        lv_obj_set_style_text_color(ui_label_status, lv_color_hex(0x44ff44), LV_PART_MAIN);
    } else {
        lv_label_set_text(ui_label_status, "CAN 发送失败!");
        lv_obj_set_style_text_color(ui_label_status, lv_color_hex(0xff4444), LV_PART_MAIN);
    }
}
