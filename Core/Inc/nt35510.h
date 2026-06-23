/**
 * @file    nt35510.h
 * @brief   NT35510 LCD 驱动 (FSMC 8080 16-bit)
 *
 * 硬件连接:
 *   FSMC Bank1 Region4 (NE4=PG12) @ 0x6C000000
 *   RS = FSMC_A6 (PF12), 16-bit 数据总线
 *   分辨率: 480×800 (竖屏), 800×480 (横屏)
 *
 * 参考: 官方驱动 tftlcd.c (NT35510 分支)
 */

#ifndef __NT35510_H
#define __NT35510_H

#include <stdint.h>

/* ================================================================
 * FSMC 地址映射
 * ================================================================
 * 8080 接口: RS=0→命令, RS=1→数据
 * struct 偏移: CMD@0x7E (RS=0), DAT@0x80 (RS=1)
 */
#define NT35510_BASE    ((uint32_t)(0x6C000000 | 0x0000007E))

typedef struct {
    uint16_t CMD;   /* 写命令寄存器 (RS=0) */
    uint16_t DAT;   /* 写数据寄存器 (RS=1) */
} NT35510_TypeDef;

#define NT35510  ((NT35510_TypeDef *)NT35510_BASE)

/* ================================================================
 * 分辨率
 * ================================================================ */
#define NT35510_WIDTH   480
#define NT35510_HEIGHT  800

/* 方向定义 */
#define NT35510_DIR_PORTRAIT   0   /* 竖屏 480×800 */
#define NT35510_DIR_LANDSCAPE  1   /* 横屏 800×480 */

/* ================================================================
 * RGB565 颜色宏
 * ================================================================ */
#define COLOR_WHITE       0xFFFF
#define COLOR_BLACK       0x0000
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_YELLOW      0xFFE0
#define COLOR_GRAY        0x8430
#define COLOR_DARKBLUE    0x01CF

/* ================================================================
 * API
 * ================================================================ */
void NT35510_Init(void);
void NT35510_SetDir(uint8_t dir);

/* 窗口 + 像素操作 */
void NT35510_SetWindow(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);
void NT35510_WritePixel(uint16_t color);          /* 快速连续写点 (SetWindow 之后) */
void NT35510_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
uint16_t NT35510_ReadPixel(uint16_t x, uint16_t y);

/* 填充 */
void NT35510_Clear(uint16_t color);
void NT35510_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);

/* 获取当前方向下的宽高 */
uint16_t NT35510_GetWidth(void);
uint16_t NT35510_GetHeight(void);

#endif /* __NT35510_H */
