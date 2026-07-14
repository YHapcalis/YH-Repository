/**
 * @file lv_conf.h
 * LVGL v8.3 配置 — MY_OTA_GUI (STM32F407ZGT6 + NT35510 800×480)
 * 基础配置保持与 v9.3 一致，格式适配 v8.3
 */

#if 1 /*Set it to "1" to enable content*/

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

/*====================
   MEMORY SETTINGS
 *====================*/

/* LVGL 内存池: 48KB 驻 CCMRAM (0x10000000, 不与 DMA 争抢) */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48U * 1024U)
#define LV_MEM_ADR 0x10000000

/*====================
   HAL SETTINGS
 *====================*/

/* 使用 HAL_GetTick() 作为 LVGL 时间基准 (TIM3 1ms) */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM == 1
#define LV_TICK_CUSTOM_INCLUDE "stm32f4xx_hal.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (HAL_GetTick())
#endif

#define LV_DISP_REFR_PERIOD 33    /* ~30fps */
#define LV_INDEV_DEF_READ_PERIOD 10

/*====================
   DISPLAY SETTINGS
 *====================*/

/* NT35510 800×480 横屏 */
#define LV_HOR_RES_MAX 800
#define VER_RES_MAX 480

/*====================
   DRAW BUFFER
 *====================*/

/* PARTIAL 模式: 16 行缓冲 (800×16×2 = 25600 字节) */
#define LV_USE_DRAW_SW 1

/*====================
   PERFORMANCE
 *====================*/

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/*====================
   FONT USAGE
 *====================*/

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_SIMSUN_16_CJK 0

/*====================
   WIDGETS
 *====================*/

#define LV_USE_BTN 1
#define LV_USE_LABEL 1
#define LV_USE_IMG 1
#define LV_USE_BAR 1
#define LV_USE_LINE 1
#define LV_USE_ARC 1
#define LV_USE_CHART 1
#define LV_USE_CONT 1
#define LV_USE_PAGE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TABLE 1
#define LV_USE_TEXTAREA 1
#define LV_USE_CANVAS 1
#define LV_USE_WIN 1
#define LV_USE_TABVIEW 1
#define LV_USE_CALENDAR 1
#define LV_USE_CALENDAR_HEADER_ARROW 1
#define LV_USE_DROPDOWN 1
#define LV_USE_LIST 1
#define LV_USE_METER 1
#define LV_USE_SPINNER 1
#define LV_USE_ANIMIMG 1

/*====================
   FILESYSTEM
 *====================*/

#define LV_USE_FS_STDIO 0
#define LV_USE_FS_RAW 0
#define LV_USE_FS_FATFS 0
#define LV_USE_FS_POSIX 0
#define LV_USE_FS_WIN32 0
#define LV_USE_FS_MEMFS 0

/* Custom SPI Flash FS via lv_fs_spi_flash.c */
#define LV_USE_FS_CUSTOM 0

/*====================
   THEME
 *====================*/

#define LV_USE_THEME_BASIC 1
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_MONO 1

/*====================
   LOG
 *====================*/

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

/*====================
   ASSERTS
 *====================*/

#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ 0

/*====================
   OTHERS
 *====================*/

#define LV_USE_OBJ_ID 0
#define LV_USE_OS 0

#endif /*LV_CONF_H*/

#endif /*End of "Content enable"*/
