/*
 * camera_ui.c — 摄像头预览屏（按需创建）
 *
 * 进入后摄像头实时画面直接写入 LCD (NT35510)，LVGL 暂停刷新。
 * 按 ← Back 回到主页后恢复 LVGL。
 * LVGL 对象在首次进入时创建，避免启动时占用 LVGL 堆。
 */

#include "camera_ui.h"
#include "lvgl.h"
#include "ov7670.h"
#include <stdio.h>

static lv_obj_t *ui_scr_camera = NULL;

volatile uint8_t g_camera_active = 0;

static void btn_back_cb(lv_event_t *e)
{
    (void)e;
    camera_ui_hide();
}

static void camera_hardware_init(void)
{
    ov_sta = 0;
    OV7670_WRST = 0; OV7670_WRST = 1;
    OV7670_RRST = 0;
    OV7670_RCK_L; OV7670_RCK_H; OV7670_RCK_L;
    OV7670_RRST = 1; OV7670_RCK_H;
    OV7670_WREN = 1;
    OV7670_CS  = 0;        /* 使能 FIFO 输出 (PG15, 低有效) */
}

/* 摄像头 UI 存在于 LVGL 堆中？ */
static int camera_ui_exists(void)
{
    return (ui_scr_camera != NULL);
}

/* 创建摄像头 LVGL 对象（首次进入时调用） */
static void camera_ui_build(void)
{
    if (ui_scr_camera) return;

    ui_scr_camera = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_scr_camera, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_scr_camera, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_scr_camera, 255, LV_PART_MAIN);

    /* 顶栏 */
    lv_obj_t *top_bar = lv_obj_create(ui_scr_camera);
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
    lv_label_set_text(lbl_back, "\xE2\x86\x90 Back");
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_back, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_center(lbl_back);

    lv_obj_t *lbl_title = lv_label_create(top_bar);
    lv_label_set_text(lbl_title, "Camera");
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x88aaff), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, 0);

    printf("[CAM] UI built\n");
}

void camera_ui_create(void)
{
    /* 空函数 — UI 改为按需创建 */
}

void camera_ui_show(void)
{
    camera_ui_build();

    if (!ui_scr_camera) {
        printf("[CAM] FAIL: memory exhausted\n");
        return;
    }

    lv_scr_load(ui_scr_camera);  /* 立即切换屏幕，不依赖动画定时器 */

    /* 初始化 OV7670 传感器 (SCCB 配置寄存器) */
    printf("[CAM] OV7670_Init...\n");
    int cam_ret = OV7670_Init();
    printf("[CAM] OV7670_Init ret=%d\n", cam_ret);

    if (cam_ret != 0) {
        printf("[CAM] Camera init FAILED - check connection\n");
        lv_obj_t *lbl = lv_label_create(ui_scr_camera);
        lv_label_set_text(lbl, "Camera not detected!");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xff4444), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_center(lbl);
        return;
    }

    camera_hardware_init();

    /* 强制清除 EXTI 挂起标志 */
    EXTI->PR = EXTI_PR_PR8;

    g_camera_active = 1;

    printf("[CAM] Preview started, ov_sta=%d\n", ov_sta);
}

void camera_ui_hide(void)
{
    g_camera_active = 0;
    OV7670_WREN = 0;

    extern lv_obj_t *ui_scr_main;
    if (ui_scr_main)
        lv_scr_load_anim(ui_scr_main, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);

    printf("[CAM] Preview stopped\n");
}
