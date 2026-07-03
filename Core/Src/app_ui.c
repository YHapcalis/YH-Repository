/*
 * app_ui.c — APP 精简 GUI (LVGL 8.3 手写)
 *
 * 基于 uint3code 32Car_APP 图片资源，适配 800×480 NT35510
 * 布局：左区仪表盘 + 右区数据面板 + 底部按钮栏
 */

#include "app_ui.h"
#include "images.h"
#include "inter_flash_cfg.h"
#include "en25q128.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx_hal.h"
#include "canif.h"
#include "mode_ui.h"
#include "settings_ui.h"
#include <stdio.h>

/* ── 自定义中文字体（SimHei 16px 子集，仅含界面所需字符）── */
LV_FONT_DECLARE(ui_font_chinese_16);

/* ── 前置函数声明 ── */
static void sysmon_timer_cb(lv_timer_t *tmr);
static void btn_ota_cb(lv_event_t *e);
static void btn_mode_cb(lv_event_t *e);
static void btn_settings_cb(lv_event_t *e);

/* ── 全局控件引用（供更新函数使用）── */
lv_obj_t *ui_scr_main;     /* 非 static — mode_ui.c 需要引用 */

/* 左区：仪表盘 */
static lv_obj_t *ui_img_gauge_bg;
static lv_obj_t *ui_img_needle;
static lv_obj_t *ui_img_light;
static lv_obj_t *ui_img_watertemp;
static lv_obj_t *ui_img_turnlight;
static lv_obj_t *ui_img_safetybelt;

/* 右区：数据面板 */
static lv_obj_t *ui_label_temp;
static lv_obj_t *ui_label_hum;
static lv_obj_t *ui_label_knob;
static lv_obj_t *ui_label_key;
static lv_obj_t *ui_label_can_status;
static lv_obj_t *ui_label_ota_status;
static lv_obj_t *ui_label_clock;

/* 指示灯颜色缓存 */
static uint8_t s_indicator_light;

/* OTA 触发标志 — GUI 主循环检测到后执行备份+复位 */
volatile uint8_t g_ota_pending = 0;

static uint8_t s_indicator_watertemp;
static uint8_t s_indicator_turnlight;
static uint8_t s_indicator_safetybelt;

/* ── 主题 ── */
static uint8_t g_theme_id = THEME_DARK;

/* Home 屏关键容器（主题切换时改底色） */
static lv_obj_t *ui_home_statusbar;
static lv_obj_t *ui_home_card1;
static lv_obj_t *ui_home_btnbar;

/* ── 内部函数声明 ── */
static void create_status_bar(void);
static void create_gauge_section(void);
static void create_data_panel(void);
static void create_button_bar(void);
static void set_indicator_color(lv_obj_t *img, uint8_t color,
                                lv_color_t c0, lv_color_t c1, lv_color_t c2);

/* ═══════════════════════════════════════════════════════════
 *  UI 创建入口
 * ═══════════════════════════════════════════════════════════ */
void app_ui_create(void)
{
    /* 主屏幕 */
    ui_scr_main = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scr_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scr_main, lv_color_hex(0x000000),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_scr_main, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    create_status_bar();      /* 顶部状态栏 */
    create_gauge_section();   /* 左区仪表盘 */
    create_data_panel();      /* 右区数据面板 */
    create_button_bar();      /* 底部按钮 */

    /* 启动系统监控定时器（1000ms 周期） */
    lv_timer_create(sysmon_timer_cb, 1000, NULL);

    lv_scr_load(ui_scr_main);
}

/* ═══════════════════════════════════════════════════════════
 *  状态栏
 * ═══════════════════════════════════════════════════════════ */
static void create_status_bar(void)
{
    /* 状态栏背景 */
    lv_obj_t *bar = lv_obj_create(ui_scr_main);
    ui_home_statusbar = bar;              /* 保存句柄供主题切换 */
    lv_obj_set_size(bar, 800, 28);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);

    /* 版本号 */
    lv_obj_t *lb = lv_label_create(bar);
    lv_label_set_text(lb, "MY_OTA_GUI  v1.0.0");
    lv_obj_set_style_text_color(lb, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lb, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lb, LV_ALIGN_LEFT_MID, 8, 0);

    /* 时钟（从 F103 RTC 接收） */
    ui_label_clock = lv_label_create(bar);
    lv_label_set_text(ui_label_clock, "--:--:--");
    lv_obj_set_style_text_color(ui_label_clock, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_clock, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(ui_label_clock, LV_ALIGN_CENTER, 0, 0);

    /* OTA 状态 */
    ui_label_ota_status = lv_label_create(bar);
    lv_label_set_text(ui_label_ota_status, "OTA: 就绪");
    lv_obj_set_style_text_color(ui_label_ota_status,
                                lv_color_hex(0x44ff44), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_ota_status,
                               &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_ota_status, LV_ALIGN_RIGHT_MID, -8, 0);

    /* CAN 状态 */
    ui_label_can_status = lv_label_create(bar);
    lv_label_set_text(ui_label_can_status, "CAN: 在线");
    lv_obj_set_style_text_color(ui_label_can_status,
                                lv_color_hex(0x44ff44), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_can_status,
                               &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_can_status, LV_ALIGN_RIGHT_MID, -130, 0);
}

/* ═══════════════════════════════════════════════════════════
 *  仪表盘（左区）
 * ═══════════════════════════════════════════════════════════ */
static void create_gauge_section(void)
{
    /* ── 仪表盘背景（233×233）── */
    ui_img_gauge_bg = lv_img_create(ui_scr_main);
    lv_img_set_src(ui_img_gauge_bg, &ui_img_942215904);
    lv_obj_set_pos(ui_img_gauge_bg, 24, 45);
    lv_obj_add_flag(ui_img_gauge_bg, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_img_gauge_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* ── 仪表指针（22×86）── */
    /* 指针轴心 (10,-30) 对齐到仪表盘中心 (141,162) */
    ui_img_needle = lv_img_create(ui_scr_main);
    lv_img_set_src(ui_img_needle, &ui_img_1601502596);
    lv_obj_set_pos(ui_img_needle, 131, 192);
    lv_img_set_pivot(ui_img_needle, 10, -30);      /* 旋转轴心 */
    lv_img_set_angle(ui_img_needle, 1500);           /* 初始角度 */
    lv_obj_add_flag(ui_img_needle, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(ui_img_needle, LV_OBJ_FLAG_SCROLLABLE);

    /* ── 指示灯 ── */
    /* 4 个图标，32×32，居中在仪表盘下方 */
    const int icon_y = 45 + 233 + 12;
    const int icon_w = 32;
    const int gap = (280 - 4 * icon_w) / 5;  /* 280px 宽度 */
    const int base_x = 24 + gap;

    ui_img_light = lv_img_create(ui_scr_main);
    lv_img_set_src(ui_img_light, &ui_img_light_png);
    lv_obj_set_pos(ui_img_light, base_x, icon_y);
    lv_obj_add_flag(ui_img_light, LV_OBJ_FLAG_ADV_HITTEST);

    ui_img_watertemp = lv_img_create(ui_scr_main);
    lv_img_set_src(ui_img_watertemp, &ui_img_temp_gray_png);
    lv_obj_set_pos(ui_img_watertemp, base_x + (icon_w + gap) * 1, icon_y);
    lv_obj_add_flag(ui_img_watertemp, LV_OBJ_FLAG_ADV_HITTEST);

    ui_img_turnlight = lv_img_create(ui_scr_main);
    lv_img_set_src(ui_img_turnlight, &ui_img_turn_light_png);
    lv_obj_set_pos(ui_img_turnlight, base_x + (icon_w + gap) * 2, icon_y);
    lv_obj_add_flag(ui_img_turnlight, LV_OBJ_FLAG_ADV_HITTEST);

    ui_img_safetybelt = lv_img_create(ui_scr_main);
    lv_img_set_src(ui_img_safetybelt, &ui_img_safety_belt_png);
    lv_obj_set_pos(ui_img_safetybelt, base_x + (icon_w + gap) * 3, icon_y);
    lv_obj_add_flag(ui_img_safetybelt, LV_OBJ_FLAG_ADV_HITTEST);
}

/* ═══════════════════════════════════════════════════════════
 *  数据面板（右区）
 * ═══════════════════════════════════════════════════════════ */
static void create_data_panel(void)
{
    const int px = 310;   /* 面板起始 X */
    const int py = 45;    /* 面板起始 Y */

    /* ── 传感器数据卡片 ── */
    lv_obj_t *card1 = lv_obj_create(ui_scr_main);
    ui_home_card1 = card1;                /* 保存句柄供主题切换 */
    lv_obj_set_size(card1, 470, 200);
    lv_obj_set_pos(card1, px, py);
    lv_obj_clear_flag(card1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card1, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_border_color(card1, lv_color_hex(0x334466), LV_PART_MAIN);
    lv_obj_set_style_border_width(card1, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(card1, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card1, 12, LV_PART_MAIN);

    /* 卡片标题 */
    lv_obj_t *title1 = lv_label_create(card1);
    lv_label_set_text(title1, "传感器数据");
    lv_obj_set_style_text_color(title1, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(title1, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(title1, LV_ALIGN_TOP_LEFT, 0, 0);

    /* 温度 */
    ui_label_temp = lv_label_create(card1);
    lv_label_set_text(ui_label_temp, "温度:   --.-°C");
    lv_obj_set_style_text_color(ui_label_temp, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_temp, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_temp, LV_ALIGN_TOP_LEFT, 0, 32);

    /* 湿度 */
    ui_label_hum = lv_label_create(card1);
    lv_label_set_text(ui_label_hum, "湿度:   --.-%");
    lv_obj_set_style_text_color(ui_label_hum, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_hum, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_hum, LV_ALIGN_TOP_LEFT, 0, 62);

    /* 旋钮 */
    ui_label_knob = lv_label_create(card1);
    lv_label_set_text(ui_label_knob, "旋钮:   ---");
    lv_obj_set_style_text_color(ui_label_knob, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_knob, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_knob, LV_ALIGN_TOP_LEFT, 0, 92);

    /* 按键 */
    ui_label_key = lv_label_create(card1);
    lv_label_set_text(ui_label_key, "按键:   ---");
    lv_obj_set_style_text_color(ui_label_key, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_label_key, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_align(ui_label_key, LV_ALIGN_TOP_LEFT, 0, 122);
}

/* ═══════════════════════════════════════════════════════════
 *  底部按钮栏
 * ═══════════════════════════════════════════════════════════ */
static void create_button_bar(void)
{
    lv_obj_t *bar = lv_obj_create(ui_scr_main);
    ui_home_btnbar = bar;                 /* 保存句柄供主题切换 */
    lv_obj_set_size(bar, 800, 36);
    lv_obj_set_pos(bar, 0, 444);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x111111), LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 4, LV_PART_MAIN);

    /* OTA 触发按钮 */
    lv_obj_t *btn_ota = lv_btn_create(bar);
    lv_obj_set_size(btn_ota, 160, 28);
    lv_obj_set_style_bg_color(btn_ota, lv_color_hex(0xcc4444), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_ota, 4, LV_PART_MAIN);
    lv_obj_align(btn_ota, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_add_event_cb(btn_ota, btn_ota_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_ota = lv_label_create(btn_ota);
    lv_label_set_text(lbl_ota, "OTA 升级");
    lv_obj_set_style_text_color(lbl_ota, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_ota, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_center(lbl_ota);

    /* Mode 按钮 */
    lv_obj_t *btn_mode = lv_btn_create(bar);
    lv_obj_set_size(btn_mode, 100, 28);
    lv_obj_set_style_bg_color(btn_mode, lv_color_hex(0x224488), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_mode, 4, LV_PART_MAIN);
    lv_obj_align(btn_mode, LV_ALIGN_LEFT_MID, 200, 0);
    lv_obj_add_event_cb(btn_mode, btn_mode_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_mode = lv_label_create(btn_mode);
    lv_label_set_text(lbl_mode, "Mode");
    lv_obj_set_style_text_color(lbl_mode, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_mode, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_center(lbl_mode);

    /* 设置按钮 */
    lv_obj_t *btn_set = lv_btn_create(bar);
    lv_obj_set_size(btn_set, 100, 28);
    lv_obj_set_style_bg_color(btn_set, lv_color_hex(0x224488), LV_PART_MAIN);
    lv_obj_set_style_radius(btn_set, 4, LV_PART_MAIN);
    lv_obj_align(btn_set, LV_ALIGN_LEFT_MID, 320, 0);
    lv_obj_add_event_cb(btn_set, btn_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_set = lv_label_create(btn_set);
    lv_label_set_text(lbl_set, "设置");
    lv_obj_set_style_text_color(lbl_set, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_set, &ui_font_chinese_16, LV_PART_MAIN);
    lv_obj_center(lbl_set);
}

/* ═══════════════════════════════════════════════════════════
 *  数据更新接口
 * ═══════════════════════════════════════════════════════════ */
void app_ui_update_sensor(float temp, float hum,
                           uint16_t knob, uint8_t key_id, uint8_t key_type)
{
    char buf[64];

    /* 温度 */
    if (temp > -50.0f && temp < 150.0f) {
        snprintf(buf, sizeof(buf), "温度:   %5.1f°C", (double)temp);
    } else {
        snprintf(buf, sizeof(buf), "温度:   ERR");
    }
    lv_label_set_text(ui_label_temp, buf);

    /* 湿度 */
    if (hum >= 0.0f && hum <= 100.0f) {
        snprintf(buf, sizeof(buf), "湿度:   %5.1f%%", (double)hum);
    } else {
        snprintf(buf, sizeof(buf), "湿度:   ERR");
    }
    lv_label_set_text(ui_label_hum, buf);

    /* 旋钮（0-255 → 仪表盘指针 0~270°） */
    snprintf(buf, sizeof(buf), "旋钮:   %3u", knob);
    lv_label_set_text(ui_label_knob, buf);

    /* 指针角度映射: knob 0→0, 128→1500, 255→3000 (LVGL角度单位: 0.1°) */
    uint32_t angle = (uint32_t)knob * 3000 / 255;
    lv_img_set_angle(ui_img_needle, angle);

    /* 按键 */
    static const char *key_names[] = {"---", "KEY1", "KEY2", "KEY3", "KEY_UP"};
    static const char *key_types[] = {"---", "PRESS", "RELEASE", "SINGLE", "DOUBLE", "LONG"};
    const char *kn = (key_id < 5) ? key_names[key_id] : "KEY?";
    const char *kt = (key_type < 6) ? key_types[key_type] : "?";
    if (key_id == 0) {
        snprintf(buf, sizeof(buf), "按键:   ---");
    } else {
        snprintf(buf, sizeof(buf), "按键:   %s_%s", kn, kt);
    }
    lv_label_set_text(ui_label_key, buf);
}

/* ═══════════════════════════════════════════════════════════
 *  时钟更新（从 F103 CAN 时间帧接收）
 * ═══════════════════════════════════════════════════════════ */
void app_ui_update_time(uint8_t year, uint8_t month, uint8_t day,
                        uint8_t hour, uint8_t min, uint8_t sec)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "20%02u-%02u-%02u  %02u:%02u:%02u",
             year, month, day, hour, min, sec);
    lv_label_set_text(ui_label_clock, buf);
}

void app_ui_set_can_status(uint8_t online)
{
    lv_label_set_text(ui_label_can_status,
                      online ? "CAN: 在线" : "CAN: 离线");
    lv_obj_set_style_text_color(ui_label_can_status,
        lv_color_hex(online ? 0x44ff44 : 0xff4444), LV_PART_MAIN);
}

void app_ui_set_ota_status(uint8_t updating)
{
    lv_label_set_text(ui_label_ota_status,
                      updating ? "OTA: 升级中" : "OTA: 就绪");
    lv_obj_set_style_text_color(ui_label_ota_status,
        lv_color_hex(updating ? 0xffaa00 : 0x44ff44), LV_PART_MAIN);
}

/* ═══════════════════════════════════════════════════════════
 *  指示灯设置
 *  0=正常(白), 1=告警(红/黄), 2=特殊
 * ═══════════════════════════════════════════════════════════ */
static void set_indicator_color(lv_obj_t *img, uint8_t color,
                                lv_color_t c0, lv_color_t c1, lv_color_t c2)
{
    lv_color_t cv;
    switch (color) {
    case 0:  cv = c0; break;
    case 1:  cv = c1; break;
    default: cv = c2; break;
    }
    lv_obj_set_style_img_recolor(img, cv, 0);
    lv_obj_set_style_img_recolor_opa(img, 255, 0);
}

void app_ui_set_indicator_light(uint8_t color)
{
    s_indicator_light = color;
    set_indicator_color(ui_img_light, color,
        lv_color_make(255, 255, 255),   /* 白: 正常 */
        lv_color_make(255, 0, 0),        /* 红: 告警 */
        lv_color_make(255, 255, 0));     /* 黄: 注意 */
}

void app_ui_set_indicator_watertemp(uint8_t color)
{
    s_indicator_watertemp = color;
    set_indicator_color(ui_img_watertemp, color,
        lv_color_make(255, 255, 255),
        lv_color_make(255, 0, 0),
        lv_color_make(0, 255, 255));
}

void app_ui_set_indicator_turnlight(uint8_t color)
{
    s_indicator_turnlight = color;
    set_indicator_color(ui_img_turnlight, color,
        lv_color_make(255, 255, 255),
        lv_color_make(255, 0, 0),
        lv_color_make(255, 255, 0));
}

void app_ui_set_indicator_safetybelt(uint8_t color)
{
    s_indicator_safetybelt = color;
    set_indicator_color(ui_img_safetybelt, color,
        lv_color_make(255, 255, 255),
        lv_color_make(255, 0, 0),
        lv_color_make(0, 255, 255));
}

/* ═══════════════════════════════════════════════════════════
 *  定时刷新：系统状态 + 模拟数据（无 CAN 时）
 * ═══════════════════════════════════════════════════════════ */
static void sysmon_timer_cb(lv_timer_t *tmr)
{
    (void)tmr;
    uint32_t now = HAL_GetTick();
    uint32_t sec = now / 1000;

    /* ── 更新 Mode 屏信息页 ── */
    uint32_t heap_free = (uint32_t)xPortGetFreeHeapSize();
    uint32_t heap_total = (uint32_t)configTOTAL_HEAP_SIZE;
    uint16_t ota_cnt = inter_flash_cfg_get_ota_count();
    uint8_t can_online = (g_can_sensor.valid && (now - g_can_sensor.tick) < 3000);
    mode_ui_update_info(heap_free, heap_total, sec, ota_cnt, can_online);

    /* ── 指示灯状态绑定 ── */
    uint32_t can_age = now - g_can_sensor.tick;

    /* 🔆 Light → CAN 连接质量：3s 内收到数据=正常，否则=告警 */
    if (g_can_sensor.valid && can_age < 3000) {
        app_ui_set_indicator_light(0);   /* 白: 正常 */
    } else {
        app_ui_set_indicator_light(1);   /* 红: 断开 */
    }

    /* 🌡 WaterTemp → F103 节点活跃：数据 10s 内=正常，否则=超时 */
    if (g_can_sensor.valid && can_age < 10000) {
        app_ui_set_indicator_watertemp(0);   /* 白: 活跃 */
    } else {
        app_ui_set_indicator_watertemp(1);   /* 红: 超时 */
    }

    /* 🚗 TurnLight → 系统心跳：每秒翻转 */
    app_ui_set_indicator_turnlight((now / 1000) & 1);

    /* 🔔 SafetyBelt → OTA 状态：由 app_ui_set_ota_status 控制 */
    /* 状态由外部调用决定，此处不覆盖 */
}

/* ═══════════════════════════════════════════════════════════
 *  主题系统
 * ═══════════════════════════════════════════════════════════ */

/* 三套主题色 — 用宏定义解决 C 静态初始化不能调 inline 函数的问题 */
#define _C(r, g, b)  LV_COLOR_MAKE(r, g, b)
static const lv_color_t theme_screen_bg[3] = {
    _C(0x00,0x00,0x00), _C(0x44,0x44,0x44), _C(0x1a,0x2e,0x1a)};
static const lv_color_t theme_card_bg[3] = {
    _C(0x11,0x11,0x11), _C(0x55,0x55,0x55), _C(0x25,0x35,0x25)};
static const lv_color_t theme_bar_bg[3] = {
    _C(0x11,0x11,0x11), _C(0x33,0x33,0x33), _C(0x1a,0x30,0x1a)};

uint8_t ui_get_theme(void)
{
    return g_theme_id;
}

void ui_apply_theme(uint8_t tid)
{
    if (tid > THEME_GREEN) return;
    g_theme_id = tid;

    /* Home 屏 */
    lv_obj_set_style_bg_color(ui_scr_main, theme_screen_bg[tid],
        LV_PART_MAIN | LV_STATE_DEFAULT);
    if (ui_home_statusbar)
        lv_obj_set_style_bg_color(ui_home_statusbar, theme_bar_bg[tid], LV_PART_MAIN);
    if (ui_home_card1)
        lv_obj_set_style_bg_color(ui_home_card1, theme_card_bg[tid], LV_PART_MAIN);
    if (ui_home_btnbar)
        lv_obj_set_style_bg_color(ui_home_btnbar, theme_bar_bg[tid], LV_PART_MAIN);

    /* 通知其他屏 */
    extern void mode_ui_apply_theme(uint8_t);
    extern void settings_ui_apply_theme(uint8_t);
    mode_ui_apply_theme(tid);
    settings_ui_apply_theme(tid);
}

/* ═══════════════════════════════════════════════════════════
 *  Mode 按钮 → 切换到 Mode 屏
 * ═══════════════════════════════════════════════════════════ */
static void btn_mode_cb(lv_event_t *e)
{
    (void)e;
    mode_ui_show();
}

static void btn_settings_cb(lv_event_t *e)
{
    (void)e;
    settings_ui_show();
}

/* ═══════════════════════════════════════════════════════════
 *  OTA 按钮事件
 * ═══════════════════════════════════════════════════════════ */
static void btn_ota_cb(lv_event_t *e)
{
    (void)e;
    printf("[GUI] OTA Trigger!\r\n");
    g_ota_pending = 1;  /* 让 GUI 主循环去执行备份+复位 */
}

