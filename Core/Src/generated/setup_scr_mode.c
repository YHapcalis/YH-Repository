/*
* Copyright 2026 NXP
* Mode screen — tileview + SRAM-preloaded small images
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"
#include "spiflash_images.h"
#include "layout_defines.h"


int mode_digital_clock_min_value = 25;
int mode_digital_clock_hour_value = 11;
int mode_digital_clock_sec_value = 50;
char mode_digital_clock_meridiem[] = "AM";

void setup_scr_mode(lv_ui *ui)
{
    printf("[MODE] setup_scr_mode start\r\n");

    // ---- mode screen ----
    ui->mode = lv_obj_create(NULL);
    lv_obj_set_size(ui->mode, MODE_W, MODE_H);
    lv_obj_set_scrollbar_mode(ui->mode, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_text_font(ui->mode, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui->mode, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->mode, lv_color_hex(0x070707), LV_PART_MAIN|LV_STATE_DEFAULT);

    // ---- tileview ----
    ui->mode_tileview = lv_tileview_create(ui->mode);
    lv_obj_set_pos(ui->mode_tileview, MODE_TILEVIEW_X, MODE_TILEVIEW_Y);
    lv_obj_set_size(ui->mode_tileview, MODE_TILEVIEW_W, MODE_TILEVIEW_H);
    lv_obj_set_scrollbar_mode(ui->mode_tileview, LV_SCROLLBAR_MODE_OFF);
    ui->mode_tileview_mode_a = lv_tileview_add_tile(ui->mode_tileview, 0, 0, LV_DIR_RIGHT);
    ui->mode_tileview_mode_f = lv_tileview_add_tile(ui->mode_tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    ui->mode_tileview_mode_c = lv_tileview_add_tile(ui->mode_tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    ui->mode_tileview_mode_e = lv_tileview_add_tile(ui->mode_tileview, 3, 0, LV_DIR_LEFT);
    lv_obj_set_style_bg_opa(ui->mode_tileview, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui->mode_tileview, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(ui->mode_tileview, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

    /* tile backgrounds — 纯 LVGL 颜色, 不用 S: 图片 */
    ui->mode_img_abg = NULL;
    lv_obj_set_style_bg_opa(ui->mode_tileview_mode_a, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->mode_tileview_mode_a, lv_color_hex(0x1a1a2e), LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_img_fbg = NULL;
    lv_obj_set_style_bg_opa(ui->mode_tileview_mode_f, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->mode_tileview_mode_f, lv_color_hex(0x16213e), LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_img_cbg = NULL;
    lv_obj_set_style_bg_opa(ui->mode_tileview_mode_c, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->mode_tileview_mode_c, lv_color_hex(0x0f3460), LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_img_ebg = NULL;
    lv_obj_set_style_bg_opa(ui->mode_tileview_mode_e, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui->mode_tileview_mode_e, lv_color_hex(0x1a1a2e), LV_PART_MAIN|LV_STATE_DEFAULT);

    // ============================================================
    // Tile A: nav 指南针
    // ============================================================
    ui->mode_label_A = lv_label_create(ui->mode_tileview_mode_a);
    lv_obj_set_pos(ui->mode_label_A, MODE_TITLE_A_X, MODE_TITLE_Y);
    lv_obj_set_size(ui->mode_label_A, MODE_TITLE_A_W, MODE_TITLE_H);
    lv_label_set_text(ui->mode_label_A, "NAV");
    lv_obj_set_style_text_align(ui->mode_label_A, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->mode_label_A, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->mode_label_A, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_img_nav = lv_image_create(ui->mode_tileview_mode_a);
    lv_obj_set_pos(ui->mode_img_nav, MODE_NAV_X, MODE_NAV_Y);
    lv_obj_set_size(ui->mode_img_nav, MODE_NAV_W, MODE_NAV_H);
    lv_image_set_src(ui->mode_img_nav, img_nav);
    lv_image_set_pivot(ui->mode_img_nav, 50, 50);
    lv_obj_set_style_image_opa(ui->mode_img_nav, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    // ============================================================
    // Tile F: temper 温度图标
    // ============================================================
    ui->mode_label_F = lv_label_create(ui->mode_tileview_mode_f);
    lv_obj_set_pos(ui->mode_label_F, MODE_TITLE_F_X, MODE_TITLE_Y);
    lv_obj_set_size(ui->mode_label_F, MODE_TITLE_F_W, MODE_TITLE_H);
    lv_label_set_text(ui->mode_label_F, "TEMP");
    lv_obj_set_style_text_align(ui->mode_label_F, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->mode_label_F, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->mode_label_F, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_img_temper = lv_image_create(ui->mode_tileview_mode_f);
    lv_obj_set_pos(ui->mode_img_temper, MODE_TEMPER_X, MODE_TEMPER_Y);
    lv_obj_set_size(ui->mode_img_temper, MODE_TEMPER_W, MODE_TEMPER_H);
    lv_image_set_src(ui->mode_img_temper, img_temper);
    lv_obj_set_style_image_opa(ui->mode_img_temper, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_label_temp_num = lv_label_create(ui->mode_tileview_mode_f);
    lv_obj_set_pos(ui->mode_label_temp_num, MODE_TEMP_NUM_X, MODE_TEMP_NUM_Y);
    lv_obj_set_size(ui->mode_label_temp_num, MODE_TEMP_NUM_W, MODE_TEMP_NUM_H);
    lv_label_set_text(ui->mode_label_temp_num, "26 C");
    lv_obj_set_style_text_font(ui->mode_label_temp_num, &lv_font_montserrat_48, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->mode_label_temp_num, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);

    // ============================================================
    // Tile C: weather 天气图标
    // ============================================================
    ui->mode_label_C = lv_label_create(ui->mode_tileview_mode_c);
    lv_obj_set_pos(ui->mode_label_C, MODE_TITLE_C_X, MODE_TITLE_Y);
    lv_obj_set_size(ui->mode_label_C, MODE_TITLE_C_W, MODE_TITLE_H);
    lv_label_set_text(ui->mode_label_C, "WEATHER");
    lv_obj_set_style_text_align(ui->mode_label_C, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->mode_label_C, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->mode_label_C, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_img_weather = lv_image_create(ui->mode_tileview_mode_c);
    lv_obj_set_pos(ui->mode_img_weather, MODE_WEATHER_X, MODE_WEATHER_Y);
    lv_obj_set_size(ui->mode_img_weather, MODE_WEATHER_W, MODE_WEATHER_H);
    lv_image_set_src(ui->mode_img_weather, img_weather);
    lv_obj_set_style_image_opa(ui->mode_img_weather, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_label_day = lv_label_create(ui->mode_tileview_mode_c);
    lv_obj_set_pos(ui->mode_label_day, MODE_DAY_X, MODE_DAY_Y);
    lv_obj_set_size(ui->mode_label_day, MODE_DAY_W, MODE_DAY_H);
    lv_label_set_text(ui->mode_label_day, "Sunny\n25 C");
    lv_obj_set_style_text_font(ui->mode_label_day, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->mode_label_day, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);

    // ============================================================
    // Tile E: music cover
    // ============================================================
    ui->mode_label_E = lv_label_create(ui->mode_tileview_mode_e);
    lv_obj_set_pos(ui->mode_label_E, MODE_TITLE_E_X, MODE_TITLE_Y);
    lv_obj_set_size(ui->mode_label_E, MODE_TITLE_E_W, MODE_TITLE_H);
    lv_label_set_text(ui->mode_label_E, "MUSIC");
    lv_obj_set_style_text_align(ui->mode_label_E, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->mode_label_E, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->mode_label_E, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_img_music = lv_image_create(ui->mode_tileview_mode_e);
    lv_obj_set_pos(ui->mode_img_music, MODE_MUSIC_X, MODE_MUSIC_Y);
    lv_obj_set_size(ui->mode_img_music, MODE_MUSIC_W, MODE_MUSIC_H);
    lv_image_set_src(ui->mode_img_music, "S:0030F338:00018880.bin");
    lv_image_set_pivot(ui->mode_img_music, 91, 91);
    lv_obj_set_style_image_opa(ui->mode_img_music, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    ui->mode_label_name = lv_label_create(ui->mode_tileview_mode_e);
    lv_obj_set_pos(ui->mode_label_name, MODE_NAME_X, MODE_NAME_Y);
    lv_obj_set_size(ui->mode_label_name, MODE_NAME_W, MODE_NAME_H);
    lv_label_set_text(ui->mode_label_name, "Go Fighting");
    lv_obj_set_style_text_align(ui->mode_label_name, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui->mode_label_name, &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui->mode_label_name, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);

    // ---- home button (screen-level overlay) ----
    ui->mode_img_home = lv_image_create(ui->mode);
    lv_obj_set_pos(ui->mode_img_home, MODE_HOME_X, MODE_HOME_Y);
    lv_obj_set_size(ui->mode_img_home, MODE_HOME_W, MODE_HOME_H);
    lv_obj_add_flag(ui->mode_img_home, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui->mode_img_home, img_home);
    lv_image_set_pivot(ui->mode_img_home, 50, 50);
    lv_obj_set_style_image_recolor_opa(ui->mode_img_home, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor(ui->mode_img_home, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_image_opa(ui->mode_img_home, 255, LV_PART_MAIN|LV_STATE_DEFAULT);

    /* nullify remaining pointers expected by gui_guider.h */
    // ui->mode_img_music set above
    ui->mode_img_1         = NULL;
    ui->mode_img_2         = NULL;
    ui->mode_img_logo      = NULL;
    ui->mode_chart_speed   = NULL;
    ui->mode_bar_battery   = NULL;
    ui->mode_digital_clock = NULL;
    ui->mode_label_WIFI    = NULL;
    ui->mode_label_BT      = NULL;
    ui->mode_label_mode    = NULL;
    ui->mode_label_dot     = NULL;
    ui->mode_label_fkm_num = NULL;
    ui->mode_label_fkm     = NULL;
    ui->mode_label_speed   = NULL;
    ui->mode_label_tempter = NULL;
    ui->mode_label_ctemp_num = NULL;
    ui->mode_label_temperature = NULL;
    // ui->mode_label_name set above
    ui->mode_label_prev    = NULL;
    ui->mode_label_play    = NULL;
    ui->mode_label_next    = NULL;
    ui->mode_label_ekm_num = NULL;
    ui->mode_label_ekm     = NULL;
    ui->mode_spangroup_1   = NULL;
    ui->mode_animimg_map   = NULL;
    ui->mode_cont          = NULL;
    ui->mode_contf         = NULL;
    ui->mode_cont_4        = NULL;
    ui->mode_contc         = NULL;
    ui->mode_conte         = NULL;

    lv_obj_update_layout(ui->mode);
    printf("[MODE] layout done\r\n");

    events_init_mode(ui);
    printf("[MODE] setup_scr_mode done\r\n");
}
