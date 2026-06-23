/**
 * @file    lv_conf.h
 * @brief   LVGL v9.5 配置文件 — MY_OTA_GUI (STM32F407ZGT6 + NT35510 800×480)
 *
 * 硬件:
 *   显示: NT35510 LCD, FSMC 8080 16-bit, 800×480 landscape, RGB565
 *   内存: 内部 SRAM 192KB (LVGL 堆 64KB) + 外部 SRAM 1MB (绘制缓冲)
 *   OS:   FreeRTOS CMSIS-V2
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/*====================
   COLOR SETTINGS
 *====================*/

/** 16-bit RGB565 — 匹配 NT35510 像素格式 */
#define LV_COLOR_DEPTH 16

/*=========================
   STDLIB WRAPPER SETTINGS
 *=========================*/

/** 使用 LVGL 内置 malloc (静态内存池, 无堆碎片) */
#define LV_USE_STDLIB_MALLOC    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING    LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_BUILTIN

#define LV_STDINT_INCLUDE       <stdint.h>
#define LV_STDDEF_INCLUDE       <stddef.h>
#define LV_STDBOOL_INCLUDE      <stdbool.h>
#define LV_INTTYPES_INCLUDE     <inttypes.h>
#define LV_LIMITS_INCLUDE       <limits.h>
#define LV_STDARG_INCLUDE       <stdarg.h>

/** LVGL 内存池: 32KB 从内部 SRAM (F407 仅 128KB 主 RAM, 64KB CCM 不可 D[MA]) */
#define LV_MEM_SIZE (32 * 1024U)
#define LV_MEM_POOL_EXPAND_SIZE 0
#define LV_MEM_ADR 0            /* 使用内置数组, 不指定外部地址 */

/*====================
   HAL SETTINGS
 *====================*/

/** 默认刷新周期 ~30fps */
#define LV_DEF_REFR_PERIOD  33

/** DPI — 4.3" 800×480 ≈ 217, 但默认值对 widget 尺寸影响不大 */
#define LV_DPI_DEF 130

/*=================
 * OPERATING SYSTEM
 *=================*/

/** FreeRTOS (原生 API, 非 CMSIS 封装 — STM32CubeMX CMSIS 缺 osThreadDetach) */
#define LV_USE_OS   LV_OS_FREERTOS

#if LV_USE_OS == LV_OS_FREERTOS
    #define LV_USE_FREERTOS_TASK_NOTIFY 1
#endif

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

/* SW 渲染引擎 (ARM Cortex-M4 无 GPU) */
#define LV_USE_DRAW_SW 1
#if LV_USE_DRAW_SW == 1
    #define LV_DRAW_SW_SUPPORT_RGB565       1
    #define LV_DRAW_SW_SUPPORT_RGB565_SWAPPED       1
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

/* 禁用其他渲染后端 (无硬件加速器) */
#define LV_USE_DRAW_OPENGL     0
#define LV_USE_DRAW_VG_LITE    0
#define LV_USE_DRAW_VITIA      0

/*========================
 * FEATURE CONFIGURATION
 *========================*/

#define LV_USE_LOG      0       /* 发布时关闭日志 */
#if LV_USE_LOG
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#endif

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

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/** 内置小内存字体 */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_48 0

#define LV_USE_FONT_PLACEHOLDER  1
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/*===================
 *  WIDGET USAGE
 *===================*/

#define LV_USE_ANIMIMG     0
#define LV_USE_ARC         1
#define LV_USE_BAR         1
#define LV_USE_BUTTON      1
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR    0
#define LV_USE_CANVAS      0
#define LV_USE_CHART       0
#define LV_USE_CHECKBOX    0
#define LV_USE_DROPDOWN    0
#define LV_USE_IMAGE       1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD    0
#define LV_USE_LABEL       1
#define LV_USE_LED         0
#define LV_USE_LINE        1
#define LV_USE_LIST        0
#define LV_USE_LOTTIE      0
#define LV_USE_MENU        0
#define LV_USE_MSGBOX      0
#define LV_USE_ROLLER      0
#define LV_USE_SCALE       1       /* 仪表盘刻度 */
#define LV_USE_SLIDER      0
#define LV_USE_SPAN        0
#define LV_USE_SPINBOX     0
#define LV_USE_SPINNER     0
#define LV_USE_SWITCH      0
#define LV_USE_TABLE       0
#define LV_USE_TABVIEW     0
#define LV_USE_TEXTAREA    0
#define LV_USE_TILEVIEW    0
#define LV_USE_WIN         0

#define LV_USE_OBSERVABLE      1
#define LV_USE_FLEX            1
#define LV_USE_GRID            1

/*=====================
 *  COMPILER SETTINGS
 *=====================*/

#define LV_ATTRIBUTE_LARGE_CONST
#define LV_ATTRIBUTE_FAST_MEM     /* IRAM 加速 */

/* 大数组 (如图片) 标记为 const → 放 Flash */
#define LV_ATTRIBUTE_LARGE_CONST

/*===================
 *  CUSTOM TICK
 *===================*/

/** 使用 FreeRTOS 系统 tick */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE "FreeRTOS.h"
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (xTaskGetTickCount())
#endif

/*=====================
 *  DEMO USAGE
 *=====================*/
#define LV_USE_DEMO_BENCHMARK    0
#define LV_USE_DEMO_MUSIC        0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_SCROLL       0
#define LV_USE_DEMO_STRESS       0
#define LV_USE_DEMO_WIDGETS      0

/*=====================
 *  OTHER
 *=====================*/
#define LV_USE_SNAPSHOT      0
#define LV_USE_MONKEY        0
#define LV_USE_GRIDNAV       0
#define LV_USE_FRAGMENT      0
#define LV_USE_IMGFONT       0
#define LV_USE_IME_PINYIN    0
#define LV_USE_FILE_EXPLORER 0

#endif /* LV_CONF_H */
