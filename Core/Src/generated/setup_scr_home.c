/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"
#include "spiflash_images.h"
#include "layout_defines.h"

/* LiberationSans 53px 字体（GUI-Guider 导出） */
extern const lv_font_t lv_font_LiberationSans_53;



int home_digital_clock_time_min_value = 25;
int home_digital_clock_time_hour_value = 11;
int home_digital_clock_time_sec_value = 50;
char home_digital_clock_time_meridiem[] = "AM";
void setup_scr_home(lv_ui *ui)
{
    //Write codes home
    ui->home = lv_obj_create(NULL);
    lv_obj_set_size(ui->home, HOME_W, HOME_H);
    lv_obj_set_scrollbar_mode(ui->home, LV_SCROLLBAR_MODE_OFF);

    //Write style for home — pure black bg (moto bg disabled for motor test)
    lv_obj_set_style_bg_opa(ui->home, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->home, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_image_src(ui->home, "S:001F5F2C:0011940C.bin", LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_image_opa(ui->home, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    // lv_obj_set_style_bg_image_recolor_opa(ui->home, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_arc_needle
    ui->home_arc_needle = lv_arc_create(ui->home);
    lv_obj_set_pos(ui->home_arc_needle, HOME_ARC_NEEDLE_X, HOME_ARC_NEEDLE_Y);
    lv_obj_set_size(ui->home_arc_needle, HOME_ARC_NEEDLE_W, HOME_ARC_NEEDLE_H);
    lv_arc_set_mode(ui->home_arc_needle, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(ui->home_arc_needle, 0, 200);
    lv_arc_set_bg_angles(ui->home_arc_needle, 130, 50);
    lv_arc_set_value(ui->home_arc_needle, 0);
    lv_arc_set_rotation(ui->home_arc_needle, 0);

    //Write style for home_arc_needle, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_arc_needle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->home_arc_needle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->home_arc_needle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_arc_needle, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->home_arc_needle, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->home_arc_needle, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->home_arc_needle, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->home_arc_needle, 10, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_arc_needle, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for home_arc_needle, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_arc_width(ui->home_arc_needle, 40, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui->home_arc_needle, 92, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui->home_arc_needle, lv_color_hex(0x17e71d), LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_arc_rounded(ui->home_arc_needle, false, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write style for home_arc_needle, Part: LV_PART_KNOB, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_arc_needle, 0, LV_PART_KNOB|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui->home_arc_needle, 0, LV_PART_KNOB|LV_STATE_DEFAULT);

    //Write codes home_label_digit
    ui->home_label_digit = lv_label_create(ui->home);
    lv_obj_set_pos(ui->home_label_digit, HOME_LABEL_DIGIT_X, HOME_LABEL_DIGIT_Y);
    lv_obj_set_size(ui->home_label_digit, HOME_LABEL_DIGIT_W, HOME_LABEL_DIGIT_H);     /* 28px 字体足够容纳 3 位数字 */
    lv_label_set_text(ui->home_label_digit, "40");
    lv_label_set_long_mode(ui->home_label_digit, LV_LABEL_LONG_WRAP);

    //Write style for home_label_digit, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->home_label_digit, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_label_digit, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_label_digit, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_label_digit, &lv_font_montserrat_32, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_label_digit, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->home_label_digit, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->home_label_digit, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_label_digit, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    // kmbg bg_image disabled (restored from reference, not in original)
    lv_obj_set_style_bg_opa(ui->home_label_digit, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_label_digit, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_label_trip_num
    ui->home_label_trip_num = lv_label_create(ui->home);
    lv_obj_set_pos(ui->home_label_trip_num, HOME_LABEL_TRIP_X, HOME_LABEL_TRIP_Y);
    lv_obj_set_size(ui->home_label_trip_num, HOME_LABEL_TRIP_W, HOME_LABEL_TRIP_H);
    lv_label_set_text(ui->home_label_trip_num, "12.4");
    lv_label_set_long_mode(ui->home_label_trip_num, LV_LABEL_LONG_WRAP);

    //Write style for home_label_trip_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_label_trip_num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_label_trip_num, &lv_font_LiberationSans_53, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_label_trip_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_label_trip_num, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_label_trip_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_label_power_num
    ui->home_label_power_num = lv_label_create(ui->home);
    lv_obj_set_pos(ui->home_label_power_num, HOME_LABEL_POWER_X, HOME_LABEL_POWER_Y);
    lv_obj_set_size(ui->home_label_power_num, HOME_LABEL_POWER_W, HOME_LABEL_POWER_H);
    lv_label_set_text(ui->home_label_power_num, "3000");
    lv_label_set_long_mode(ui->home_label_power_num, LV_LABEL_LONG_WRAP);

    //Write style for home_label_power_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_label_power_num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_label_power_num, &lv_font_LiberationSans_53, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_label_power_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_label_power_num, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_label_power_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_digital_clock_time
    static bool home_digital_clock_time_timer_enabled = false;
    ui->home_digital_clock_time = lv_label_create(ui->home);
    lv_obj_set_pos(ui->home_digital_clock_time, HOME_CLOCK_X, HOME_CLOCK_Y);
    lv_obj_set_size(ui->home_digital_clock_time, HOME_CLOCK_W, HOME_CLOCK_H);
    lv_label_set_text(ui->home_digital_clock_time, "11:25 AM");
    if (!home_digital_clock_time_timer_enabled) {
        lv_timer_create(home_digital_clock_time_timer, 1000, NULL);
        home_digital_clock_time_timer_enabled = true;
    }

    //Write style for home_digital_clock_time, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_radius(ui->home_digital_clock_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_digital_clock_time, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_digital_clock_time, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_digital_clock_time, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->home_digital_clock_time, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_digital_clock_time, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->home_digital_clock_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->home_digital_clock_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->home_digital_clock_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->home_digital_clock_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->home_digital_clock_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_digital_clock_time, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_bar_battery
    ui->home_bar_battery = lv_bar_create(ui->home);
    lv_obj_set_pos(ui->home_bar_battery, HOME_BATTERY_X, HOME_BATTERY_Y);
    lv_obj_set_size(ui->home_bar_battery, HOME_BATTERY_W, HOME_BATTERY_H);
    lv_obj_set_style_anim_duration(ui->home_bar_battery, 1000, 0);
    lv_bar_set_mode(ui->home_bar_battery, LV_BAR_MODE_NORMAL);
    lv_bar_set_range(ui->home_bar_battery, 0, 100);
    lv_bar_set_value(ui->home_bar_battery, 90, LV_ANIM_OFF);

    //Write style for home_bar_battery, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_bar_battery, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_bar_battery, 17, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_bar_battery, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui->home_bar_battery, NULL, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_opa(ui->home_bar_battery, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_recolor_opa(ui->home_bar_battery, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write style for home_bar_battery, Part: LV_PART_INDICATOR, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_bar_battery, 0, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_bar_battery, 17, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_src(ui->home_bar_battery, NULL, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_opa(ui->home_bar_battery, 255, LV_PART_INDICATOR|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_image_recolor_opa(ui->home_bar_battery, 0, LV_PART_INDICATOR|LV_STATE_DEFAULT);

    //Write codes home_label_ODO_num
    ui->home_label_ODO_num = lv_label_create(ui->home);
    lv_obj_set_pos(ui->home_label_ODO_num, HOME_ODO_X, HOME_ODO_Y);
    lv_obj_set_size(ui->home_label_ODO_num, HOME_ODO_W, HOME_ODO_H);
    lv_label_set_text(ui->home_label_ODO_num, "300");
    lv_label_set_long_mode(ui->home_label_ODO_num, LV_LABEL_LONG_WRAP);

    //Write style for home_label_ODO_num, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_label_ODO_num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_label_ODO_num, &lv_font_LiberationSans_53, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_label_ODO_num, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_label_ODO_num, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_label_ODO_num, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_img_right
    ui->home_img_right = lv_image_create(ui->home);
    lv_obj_set_pos(ui->home_img_right, HOME_RIGHT_X, HOME_RIGHT_Y);
    lv_obj_set_size(ui->home_img_right, HOME_RIGHT_W, HOME_RIGHT_H);
    lv_obj_add_flag(ui->home_img_right, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->home_img_right, img_direction);
    lv_image_set_pivot(ui->home_img_right, 50,50);
    lv_image_set_rotation(ui->home_img_right, 0);

    //Write style for home_img_right, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->home_img_right, 1, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(ui->home_img_right, lv_color_hex(0x04cf00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->home_img_right, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_img_left
    ui->home_img_left = lv_image_create(ui->home);
    lv_obj_set_pos(ui->home_img_left, HOME_LEFT_X, HOME_LEFT_Y);
    lv_obj_set_size(ui->home_img_left, HOME_LEFT_W, HOME_LEFT_H);
    lv_obj_add_flag(ui->home_img_left, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->home_img_left, img_direction);
    lv_image_set_pivot(ui->home_img_left, 12,12);
    lv_image_set_rotation(ui->home_img_left, 1800);

    //Write style for home_img_left, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->home_img_left, 239, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(ui->home_img_left, lv_color_hex(0x04cf00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->home_img_left, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_label_WIFI
    ui->home_label_WIFI = lv_label_create(ui->home);
    lv_obj_set_pos(ui->home_label_WIFI, HOME_WIFI_X, HOME_WIFI_Y);
    lv_obj_set_size(ui->home_label_WIFI, HOME_WIFI_W, HOME_WIFI_H);
    lv_label_set_text(ui->home_label_WIFI, "" LV_SYMBOL_WIFI "");
    lv_label_set_long_mode(ui->home_label_WIFI, LV_LABEL_LONG_WRAP);

    //Write style for home_label_WIFI, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_label_WIFI, lv_color_hex(0x14ff00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_label_WIFI, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_label_WIFI, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->home_label_WIFI, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_label_WIFI, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_label_WIFI, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_label_BT
    ui->home_label_BT = lv_label_create(ui->home);
    lv_obj_set_pos(ui->home_label_BT, HOME_BT_X, HOME_BT_Y);
    lv_obj_set_size(ui->home_label_BT, HOME_BT_W, HOME_BT_H);
    lv_label_set_text(ui->home_label_BT, "" LV_SYMBOL_BLUETOOTH " ");
    lv_label_set_long_mode(ui->home_label_BT, LV_LABEL_LONG_WRAP);

    //Write style for home_label_BT, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_border_width(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_label_BT, lv_color_hex(0x3101fe), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_label_BT, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_label_BT, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(ui->home_label_BT, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_line_space(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_label_BT, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_label_BT, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_img_moto
    ui->home_img_moto = lv_image_create(ui->home);
    lv_obj_set_pos(ui->home_img_moto, HOME_MOTO_X, HOME_MOTO_Y);
    lv_obj_set_size(ui->home_img_moto, HOME_MOTO_W, HOME_MOTO_H);
    lv_obj_add_flag(ui->home_img_moto, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->home_img_moto, img_monitor);
    lv_image_set_pivot(ui->home_img_moto, 50,50);
    lv_image_set_rotation(ui->home_img_moto, 0);

    //Write style for home_img_moto, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->home_img_moto, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(ui->home_img_moto, lv_color_hex(0xf3ff00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->home_img_moto, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_img_light
    ui->home_img_light = lv_image_create(ui->home);
    lv_obj_set_pos(ui->home_img_light, HOME_LIGHT_X, HOME_LIGHT_Y);
    lv_obj_set_size(ui->home_img_light, HOME_LIGHT_W, HOME_LIGHT_H);
    lv_obj_add_flag(ui->home_img_light, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->home_img_light, img_light);
    lv_image_set_pivot(ui->home_img_light, 50,50);
    lv_image_set_rotation(ui->home_img_light, 0);

    //Write style for home_img_light, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->home_img_light, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->home_img_light, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_btn_mode_a
    ui->home_btn_mode_a = lv_button_create(ui->home);
    lv_obj_set_pos(ui->home_btn_mode_a, HOME_BTN_X_A, HOME_BTN_Y);
    lv_obj_set_size(ui->home_btn_mode_a, HOME_BTN_W, HOME_BTN_H);
    ui->home_btn_mode_a_label = lv_label_create(ui->home_btn_mode_a);
    lv_label_set_text(ui->home_btn_mode_a_label, "A");
    lv_label_set_long_mode(ui->home_btn_mode_a_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->home_btn_mode_a_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->home_btn_mode_a, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->home_btn_mode_a_label, LV_PCT(100));

    //Write style for home_btn_mode_a, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_btn_mode_a, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->home_btn_mode_a, lv_color_hex(0x2d373d), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->home_btn_mode_a, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->home_btn_mode_a, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_btn_mode_a, 7, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_btn_mode_a, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_btn_mode_a, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_btn_mode_a, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_btn_mode_a, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_btn_mode_a, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_img_red
    ui->home_img_red = lv_image_create(ui->home);
    lv_obj_set_pos(ui->home_img_red, HOME_RED_X, HOME_RED_Y);
    lv_obj_set_size(ui->home_img_red, HOME_RED_W, HOME_RED_H);
    lv_obj_add_flag(ui->home_img_red, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->home_img_red, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->home_img_red, NULL);
    lv_image_set_pivot(ui->home_img_red, 50,50);
    lv_image_set_rotation(ui->home_img_red, 0);

    //Write style for home_img_red, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->home_img_red, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->home_img_red, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_img_green
    ui->home_img_green = lv_image_create(ui->home);
    lv_obj_set_pos(ui->home_img_green, HOME_GREEN_X, HOME_GREEN_Y);
    lv_obj_set_size(ui->home_img_green, HOME_GREEN_W, HOME_GREEN_H);
    lv_obj_add_flag(ui->home_img_green, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->home_img_green, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->home_img_green, NULL);
    lv_image_set_pivot(ui->home_img_green, 50,50);
    lv_image_set_rotation(ui->home_img_green, 0);

    //Write style for home_img_green, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->home_img_green, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->home_img_green, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_btn_mode_f
    ui->home_btn_mode_f = lv_button_create(ui->home);
    lv_obj_set_pos(ui->home_btn_mode_f, HOME_BTN_X_F, HOME_BTN_Y);
    lv_obj_set_size(ui->home_btn_mode_f, HOME_BTN_W, HOME_BTN_H);
    ui->home_btn_mode_f_label = lv_label_create(ui->home_btn_mode_f);
    lv_label_set_text(ui->home_btn_mode_f_label, "F");
    lv_label_set_long_mode(ui->home_btn_mode_f_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->home_btn_mode_f_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->home_btn_mode_f, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->home_btn_mode_f_label, LV_PCT(100));

    //Write style for home_btn_mode_f, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_btn_mode_f, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->home_btn_mode_f, lv_color_hex(0x018e94), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->home_btn_mode_f, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->home_btn_mode_f, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_btn_mode_f, 7, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_btn_mode_f, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_btn_mode_f, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_btn_mode_f, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_btn_mode_f, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_btn_mode_f, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_btn_mode_e
    ui->home_btn_mode_e = lv_button_create(ui->home);
    lv_obj_set_pos(ui->home_btn_mode_e, HOME_BTN_X_E, HOME_BTN_Y);
    lv_obj_set_size(ui->home_btn_mode_e, HOME_BTN_W, HOME_BTN_H);
    ui->home_btn_mode_e_label = lv_label_create(ui->home_btn_mode_e);
    lv_label_set_text(ui->home_btn_mode_e_label, "E");
    lv_label_set_long_mode(ui->home_btn_mode_e_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->home_btn_mode_e_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->home_btn_mode_e, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->home_btn_mode_e_label, LV_PCT(100));

    //Write style for home_btn_mode_e, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_btn_mode_e, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->home_btn_mode_e, lv_color_hex(0x7b8a02), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->home_btn_mode_e, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->home_btn_mode_e, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_btn_mode_e, 7, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_btn_mode_e, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_btn_mode_e, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_btn_mode_e, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_btn_mode_e, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_btn_mode_e, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_img_high_light
    ui->home_img_high_light = lv_image_create(ui->home);
    lv_obj_set_pos(ui->home_img_high_light, HOME_HIGH_LIGHT_X, HOME_HIGH_LIGHT_Y);
    lv_obj_set_size(ui->home_img_high_light, HOME_HIGH_LIGHT_W, HOME_HIGH_LIGHT_H);
    lv_obj_add_flag(ui->home_img_high_light, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->home_img_high_light, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->home_img_high_light, img_high_beam);
    lv_image_set_pivot(ui->home_img_high_light, 50,50);
    lv_image_set_rotation(ui->home_img_high_light, 0);

    //Write style for home_img_high_light, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_image_recolor_opa(ui->home_img_high_light, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(ui->home_img_high_light, lv_color_hex(0x00edfe), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->home_img_high_light, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    //Write codes home_btn_mode_c
    ui->home_btn_mode_c = lv_button_create(ui->home);
    lv_obj_set_pos(ui->home_btn_mode_c, HOME_BTN_X_C, HOME_BTN_Y);
    lv_obj_set_size(ui->home_btn_mode_c, HOME_BTN_W, HOME_BTN_H);
    ui->home_btn_mode_c_label = lv_label_create(ui->home_btn_mode_c);
    lv_label_set_text(ui->home_btn_mode_c_label, "C");
    lv_label_set_long_mode(ui->home_btn_mode_c_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->home_btn_mode_c_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->home_btn_mode_c, 0, LV_STATE_DEFAULT);
    lv_obj_set_width(ui->home_btn_mode_c_label, LV_PCT(100));

    //Write style for home_btn_mode_c, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
    lv_obj_set_style_bg_opa(ui->home_btn_mode_c, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->home_btn_mode_c, lv_color_hex(0x508602), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui->home_btn_mode_c, LV_GRAD_DIR_NONE, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui->home_btn_mode_c, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->home_btn_mode_c, 7, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->home_btn_mode_c, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->home_btn_mode_c, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->home_btn_mode_c, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui->home_btn_mode_c, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui->home_btn_mode_c, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);

    //The custom code of home.


    //Update current screen layout.
    lv_obj_update_layout(ui->home);

    //Init events for screen.
    events_init_home(ui);
}
