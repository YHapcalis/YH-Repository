/**
 * @file    lv_conf.h
 * @brief   LVGL v9.3 配置 — MY_OTA_GUI (STM32F407ZGT6 + NT35510 800×480)
 *          GUI-Guider 生成代码配套, 基于 v9.3 lv_conf_template.h
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/*====================
   COLOR SETTINGS
 *====================*/

#define LV_COLOR_DEPTH 16

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>

/** LVGL 内存池: 48KB 驻 CCMRAM (0x10000000, 不与 DMA 争抢) */
#define LV_MEM_SIZE (48 * 1024U)
#define LV_MEM_POOL_EXPAND_SIZE 0
#define LV_MEM_ADR 0x10000000

/*====================
   HAL SETTINGS
 *====================*/

#define LV_DEF_REFR_PERIOD  33
#define LV_DPI_DEF 130

/*=================
 * OPERATING SYSTEM
 *=================*/

#define LV_USE_OS   LV_OS_NONE       /* 裸 LVGL + FreeRTOS 任务中轮询 lv_timer_handler() */

/*========================
 * RENDERING CONFIGURATION
 *========================*/

#define LV_DRAW_BUF_STRIDE_ALIGN                1
#define LV_DRAW_BUF_ALIGN                       4
#define LV_DRAW_TRANSFORM_USE_MATRIX            0
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE    (24 * 1024)
#define LV_DRAW_LAYER_MAX_MEMORY 0
#define LV_DRAW_THREAD_STACK_SIZE    (8 * 1024)
#define LV_DRAW_THREAD_PRIO LV_THREAD_PRIO_HIGH

/* SW 渲染引擎 */
#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW == 1
    #define LV_DRAW_SW_SUPPORT_RGB565       1
    #define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED 1
    #define LV_DRAW_SW_SUPPORT_RGB565A8     1
    #define LV_DRAW_SW_SUPPORT_RGB888       1
    #define LV_DRAW_SW_SUPPORT_XRGB8888     1
    #define LV_DRAW_SW_SUPPORT_ARGB8888     1
    #define LV_DRAW_SW_SUPPORT_ARGB8888_PREMULTIPLIED 1
    #define LV_DRAW_SW_SUPPORT_L8           1
    #define LV_DRAW_SW_SUPPORT_AL88         1
    #define LV_DRAW_SW_SUPPORT_A8           1
    #define LV_DRAW_SW_SUPPORT_I1           1

    #define LV_DRAW_SW_I1_LUM_THRESHOLD 127
    #define LV_DRAW_SW_DRAW_UNIT_CNT    1
    #define LV_USE_DRAW_ARM2D_SYNC      0
#endif

#define LV_USE_DRAW_VG_LITE    0
#define LV_USE_DRAW_VITIA      0
#define LV_USE_DRAW_OPENGL     0

/*========================
 * FEATURE CONFIGURATION
 *========================*/

#define LV_USE_LOG      0

#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0

#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      0
#define LV_USE_REFR_DEBUG       0
#define LV_USE_SYSMON           0
#define LV_USE_PARALLEL_DRAW_DEBUG 0

#define LV_CACHE_DEF_SIZE       0

/*================
 *  FONT USAGE
 *================*/

#define LV_FONT_DEFAULT &lv_font_montserrat_28

#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_48 0

#define LV_USE_FONT_PLACEHOLDER  1
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/*===================
 *  WIDGET USAGE
 *===================*/

#define LV_USE_ANIMIMG     1       /* GUI-Guider mode screen 用 */
#define LV_USE_ARC         1
#define LV_USE_BAR         1
#define LV_USE_BUTTON      1
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR    0
#define LV_USE_CANVAS      0
#define LV_USE_CHART       1       /* GUI-Guider mode screen 用 */
#define LV_USE_CHECKBOX    0
#define LV_USE_DROPDOWN    0
#define LV_USE_IMAGE       1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD    0
#define LV_USE_LABEL       1
#define LV_USE_LED         0
#define LV_USE_LINE        0
#define LV_USE_LIST        0
#define LV_USE_LOTTIE      0
#define LV_USE_MENU        0
#define LV_USE_MSGBOX      0
#define LV_USE_ROLLER      0
#define LV_USE_SCALE       0
#define LV_USE_SLIDER      0
#define LV_USE_SPAN        1       /* GUI-Guider mode screen 用 */
#define LV_USE_SPINBOX     0
#define LV_USE_SPINNER     0
#define LV_USE_SWITCH      0
#define LV_USE_TABLE       0
#define LV_USE_TABVIEW     0
#define LV_USE_TEXTAREA    0
#define LV_USE_TILEVIEW    1       /* GUI-Guider mode screen 用 */
#define LV_USE_WIN         0

#define LV_USE_OBSERVABLE      0
#define LV_USE_FLEX            1
#define LV_USE_GRID            1

/* 符号字体 (GUI-Guider 音乐面板用) */
#define LV_USE_SYMBOL_PLAY     1
#define LV_USE_SYMBOL_PAUSE    1

/*=====================
 *  COMPILER SETTINGS
 *=====================*/

#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_FAST_MEM

/*===================
 *  CUSTOM TICK
 *===================*/

/** 使用 HAL tick (TIM3 时基, 已配置) */
#define LV_TICK_CUSTOM 0

/*=====================
 *  GUI-Guider 扩展
 *=====================*/

/* lv_conf_ext.h 中覆盖的宏 */
#define LV_USE_FLOAT 1
#define LV_USE_GUIDER_SIMULATOR 0

#include "lv_conf_ext.h"

/*=====================
 *  DEMO USAGE
 *=====================*/

#define LV_USE_DEMO_BENCHMARK    0
#define LV_USE_DEMO_MUSIC        0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_SCROLL       0
#define LV_USE_DEMO_STRESS       0
#define LV_USE_DEMO_WIDGETS      0

#endif /* LV_CONF_H */
