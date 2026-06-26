/**
 * @file    spiflash_images.h
 * @brief   SPI Flash 图片加载 — SRAM 预加载 (≤256KB 安全边界)
 */

#ifndef __SPIFLASH_IMAGES_H
#define __SPIFLASH_IMAGES_H

#include "lvgl.h"

void spiflash_images_init(void);

extern const lv_image_dsc_t *img_kmbg;
extern const lv_image_dsc_t *img_battery_bak;
extern const lv_image_dsc_t *img_battery_ind;
extern const lv_image_dsc_t *img_direction;
extern const lv_image_dsc_t *img_light;
extern const lv_image_dsc_t *img_high_beam;
extern const lv_image_dsc_t *img_home;
extern const lv_image_dsc_t *img_logo;
extern const lv_image_dsc_t *img_monitor;
extern const lv_image_dsc_t *img_nav;
extern const lv_image_dsc_t *img_temper;
extern const lv_image_dsc_t *img_weather;

#endif
