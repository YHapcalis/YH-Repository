/*
* Copyright 2024 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include <stdlib.h>
#include "lvgl.h"
#include "custom.h"
#include "canif.h"
#include "DRV8833.h"
#include "SG-90.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static int32_t speed = 39;
static int32_t power = 3000;
static float trip = 12.4;
static int32_t ODO = 288;
static int music_status = 0;
static bool is_increase = true;
static uint32_t track_id = 1;
static const char * music_list[] = {
    "Go Fighting",
    "Because Of You",
    "Need a Better Future"
};

extern lv_ui guider_ui;
/**
 * Create a demo application
 */

/* set the digital label and steering lamp image style. */
static void set_position_x(void * gui, int32_t temp)
{
    lv_obj_set_x(gui, temp);
    if(temp >= 193) {
        lv_obj_set_style_img_recolor_opa(guider_ui.home_img_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_add_flag(guider_ui.home_img_light, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(guider_ui.home_img_high_light, LV_OBJ_FLAG_HIDDEN);
        if(temp % 4 == 0) {
            lv_obj_set_style_img_recolor_opa(guider_ui.home_img_right, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_img_recolor_opa(guider_ui.home_img_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
        }
    } else {
        lv_obj_set_style_img_recolor_opa(guider_ui.home_img_right, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_clear_flag(guider_ui.home_img_light, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(guider_ui.home_img_high_light, LV_OBJ_FLAG_HIDDEN);
        if(temp % 4 == 0) {
            lv_obj_set_style_img_recolor_opa(guider_ui.home_img_left, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
        } else {
            lv_obj_set_style_img_recolor_opa(guider_ui.home_img_left, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
        }
    }
}

static void set_position_y(void * gui, int32_t temp)
{
    lv_obj_set_y(gui, temp);
}

void custom_init(lv_ui *ui)
{
    /* Add your codes here */
}

/* ═══════════════════════ Mode 屏温度更新 ═══════════════════════ */

void mode_temp_update_cb(lv_timer_t *t)
{
    (void)t;
    /* 温湿度显示已注释 — 当前聚焦舵机/电机控制 */
    (void)g_can_sensor;
}

/* ═══════════════════════ 旋钮 → 转向灯 ═══════════════════════ */

/* 转向状态 */
#define TURN_NONE   0
#define TURN_LEFT   1
#define TURN_RIGHT  2

#define TURN_BLINK_MS   500  /* 闪烁周期 */
#define TURN_TIMEOUT_MS 3000 /* 无操作后自动熄灭 */

static uint8_t  s_turn_dir       = TURN_NONE;
static uint8_t  s_knob_prev      = 128;
static uint8_t  s_turn_inited    = 0;
static uint32_t s_turn_act_tick  = 0;

void home_turn_signal_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t now = HAL_GetTick();

    /* 首次运行：隐藏两个转向灯（setup_scr_home 默认是可见的） */
    if (!s_turn_inited) {
        s_turn_inited = 1;
        if (guider_ui.home_img_right)
            lv_obj_add_flag(guider_ui.home_img_right, LV_OBJ_FLAG_HIDDEN);
        if (guider_ui.home_img_left)
            lv_obj_add_flag(guider_ui.home_img_left, LV_OBJ_FLAG_HIDDEN);
    }

    /* ── 仅 home 屏活动时运行 ── */
    if (lv_screen_active() != guider_ui.home)
        return;

    /* ── 检测旋钮变化 ── */
    if (g_can_sensor.valid) {
        uint8_t kv = g_can_sensor.knob;
        int16_t diff = (int16_t)kv - (int16_t)s_knob_prev;

        if (diff > 3) {
            s_turn_dir = TURN_LEFT;
            s_turn_act_tick = now;
        } else if (diff < -3) {
            s_turn_dir = TURN_RIGHT;
            s_turn_act_tick = now;
        }
        s_knob_prev = kv;
    }

    /* ── 超时熄灭 ── */
    if (s_turn_dir != TURN_NONE && (now - s_turn_act_tick >= TURN_TIMEOUT_MS)) {
        s_turn_dir = TURN_NONE;
        lv_obj_add_flag(guider_ui.home_img_right, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(guider_ui.home_img_left,  LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* ── 闪烁控制 ── */
    if (s_turn_dir != TURN_NONE) {
        uint8_t on = ((now / TURN_BLINK_MS) % 2 == 0);

        if (s_turn_dir == TURN_RIGHT) {
            if (on) {
                lv_obj_clear_flag(guider_ui.home_img_right, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_img_recolor_opa(guider_ui.home_img_right, 239,
                    LV_PART_MAIN|LV_STATE_DEFAULT);
                lv_obj_set_style_img_recolor(guider_ui.home_img_right,
                    lv_color_hex(0x04cf00), LV_PART_MAIN|LV_STATE_DEFAULT);
            } else {
                lv_obj_add_flag(guider_ui.home_img_right, LV_OBJ_FLAG_HIDDEN);
            }
        } else { /* TURN_LEFT */
            if (on) {
                lv_obj_clear_flag(guider_ui.home_img_left, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_img_recolor_opa(guider_ui.home_img_left, 239,
                    LV_PART_MAIN|LV_STATE_DEFAULT);
                lv_obj_set_style_img_recolor(guider_ui.home_img_left,
                    lv_color_hex(0x04cf00), LV_PART_MAIN|LV_STATE_DEFAULT);
            } else {
                lv_obj_add_flag(guider_ui.home_img_left, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

/* ═══════════════════════ F407 按键 → 舵机/电机控制 ═══════════════════════ */

#include "f407_key.h"

static uint8_t s_servo_abs_angle = 90;  /* 舵机绝对位置 0~180° */

void f407_key_motor_servo_cb(lv_timer_t *t)
{
    (void)t;

    /* ── 扫描按键 ── */
    F407_Key_Task();

    /* ── 消费按键事件 ── */
    F407_KeyEvent evt;
    while (F407_Key_PopEvent(&evt)) {
        switch (evt.id) {

        case F407_KEY_UP:          /* PA0 → 电机正转 */
            if (evt.type == F407_KEY_EVENT_SINGLE) {
                DRV8833_Forward(80);
                printf("[KEY] UP → FWD 80%%\r\n");
            }
            break;

        case F407_KEY_1:           /* PE3 → 电机停止 */
            if (evt.type == F407_KEY_EVENT_SINGLE) {
                DRV8833_Stop();
                printf("[KEY] 1 → STOP\r\n");
            }
            break;

        case F407_KEY_0:           /* PE4 → 舵机右 */
            if (evt.type == F407_KEY_EVENT_SINGLE) {
                if (s_servo_abs_angle < 170) s_servo_abs_angle += 10;
            } else if (evt.type == F407_KEY_EVENT_LONG) {
                s_servo_abs_angle = 170;  /* 长按 → 满舵右 */
            }
            SG90_SetAngle((int8_t)s_servo_abs_angle - 90);
            printf("[KEY] 0 → Servo R (abs=%u)\r\n", s_servo_abs_angle);
            break;

        case F407_KEY_2:           /* PE2 → 舵机左 */
            if (evt.type == F407_KEY_EVENT_SINGLE) {
                if (s_servo_abs_angle > 10) s_servo_abs_angle -= 10;
            } else if (evt.type == F407_KEY_EVENT_LONG) {
                s_servo_abs_angle = 10;   /* 长按 → 满舵左 */
            }
            SG90_SetAngle((int8_t)s_servo_abs_angle - 90);
            printf("[KEY] 2 → Servo L (abs=%u)\r\n", s_servo_abs_angle);
            break;

        default:
            break;
        }
    }

    /* 舵机角度平滑更新 */
    SG90_Update();
}

/* 创建所有 custom 定时器（由 StartGUITask 调用） */
void custom_start_timers(lv_ui *ui)
{
    lv_timer_create(mode_temp_update_cb, 500, ui);
    lv_timer_create(home_turn_signal_cb, 100, ui);  /* 100ms 保证顺滑闪烁 */
    lv_timer_create(f407_key_motor_servo_cb, 20, ui);  /* 50Hz 按键扫描 + 舵机平滑 */
}

void speed_meter_timer_cb(lv_timer_t * t)
{
    lv_ui * gui = lv_timer_get_user_data(t);

    if(speed >= 160) is_increase = false;
    if(speed <= 30) is_increase = true;

    lv_arc_set_value(gui->home_arc_needle, speed);
    lv_label_set_text_fmt(gui->home_label_digit, "%"LV_PRId32, speed);
    // update trip value
    if(trip < 200.0) trip += 0.001;
    lv_label_set_text_fmt(gui->home_label_trip_num, "%4.1f", trip);
    // update power value
    lv_label_set_text_fmt(gui->home_label_power_num, "%"LV_PRId32, power + speed);
    // update ODO value
    lv_label_set_text_fmt(gui->home_label_ODO_num, "%"LV_PRId32, ODO + (int)trip);
    if(is_increase)
    {
        speed += speed / 20;
    }else
    {
        speed -= speed / 10;
    }
    if(speed <= 60) {
        lv_obj_set_style_arc_color(gui->home_arc_needle, lv_color_hex(0x209808), LV_PART_INDICATOR|LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_recolor(gui->home_label_digit, lv_color_hex(0x209808), LV_PART_MAIN|LV_STATE_DEFAULT);
	    lv_obj_set_style_bg_img_recolor_opa(gui->home_label_digit, 246, LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(gui->home_img_moto, lv_color_hex(0x209808), LV_PART_MAIN|LV_STATE_DEFAULT);
    }else if (speed > 60 && speed <=120){
        lv_obj_set_style_arc_color(gui->home_arc_needle, lv_color_hex(0xd6e10b), LV_PART_INDICATOR|LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_recolor(gui->home_label_digit, lv_color_hex(0xd6e10b), LV_PART_MAIN|LV_STATE_DEFAULT);
	    lv_obj_set_style_bg_img_recolor_opa(gui->home_label_digit, 246, LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(gui->home_img_moto, lv_color_hex(0xd6e10b), LV_PART_MAIN|LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_arc_color(gui->home_arc_needle, lv_color_hex(0xcc1607), LV_PART_INDICATOR|LV_STATE_DEFAULT);
        lv_obj_set_style_bg_img_recolor(gui->home_label_digit, lv_color_hex(0xcc1607), LV_PART_MAIN|LV_STATE_DEFAULT);
	    lv_obj_set_style_bg_img_recolor_opa(gui->home_label_digit, 246, LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(gui->home_img_moto, lv_color_hex(0xcc1607), LV_PART_MAIN|LV_STATE_DEFAULT);
    }
}

void home_label_digit_animation(lv_ui *ui)
{
    lv_anim_t a, b;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, set_position_x);
    lv_anim_set_time(&a, 3000);
    lv_anim_set_playback_time(&a, 2000);
    lv_anim_set_delay(&a, 500);
    lv_anim_set_var(&a, ui->home_label_digit);
    lv_anim_set_values(&a, 295, 335);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
    lv_anim_init(&b);
    lv_anim_set_exec_cb(&b, set_position_y);
    lv_anim_set_time(&b, 4000);
    lv_anim_set_playback_time(&b, 3000);
    lv_anim_set_delay(&b, 500);
    lv_anim_set_var(&b, ui->home_label_digit);
    lv_anim_set_values(&b, 160, 180);
    lv_anim_set_repeat_count(&b, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&b);
}

void play_music(lv_ui *ui)
{
    if(music_status == 0) {
        lv_label_set_text(ui->mode_label_play, "" LV_SYMBOL_PAUSE "");
        music_status = 1;
        lv_anim_resume(lv_anim_get(guider_ui.mode_img_music, (lv_anim_exec_xcb_t)lv_img_set_angle));
        lv_label_set_long_mode(guider_ui.mode_label_name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }else {
        music_status = 0;
        lv_label_set_text(ui->mode_label_play, "" LV_SYMBOL_PLAY "");
        lv_anim_pause(lv_anim_get(guider_ui.mode_img_music, (lv_anim_exec_xcb_t)lv_img_set_angle));
        lv_label_set_long_mode(guider_ui.mode_label_name, LV_LABEL_LONG_CLIP);
    }
}

static const void * lv_demo_music_get_list_img(uint32_t track_id)
{
    switch (track_id)
    {
    case 0:
        return NULL;
        break;
    case 1:
        return NULL;
        break;
    case 2:
        return NULL;
        break;
    default:
        return NULL;
        break;
    }
}

void music_album_next(bool next)
{
    if(next) {
        track_id++;
        if(track_id >= 3) track_id = 0;
    } else {
        if(track_id == 0) {
            track_id = 2;
        } else {
            track_id--;
        }
    }
    lv_img_set_src(guider_ui.mode_img_music, lv_demo_music_get_list_img(track_id));
    lv_img_set_angle(guider_ui.mode_img_music, 0);
    lv_label_set_text(guider_ui.mode_label_name, music_list[track_id]);
    if(music_status){
        lv_label_set_long_mode(guider_ui.mode_label_name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    } else {
        lv_label_set_long_mode(guider_ui.mode_label_name, LV_LABEL_LONG_CLIP);
    }
}

