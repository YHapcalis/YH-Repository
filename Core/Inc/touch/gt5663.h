/**
 * @file    gt5663.h
 * @brief   GT5663/GT5688 电容触摸驱动 (软件 I2C, 7-bit addr=0x14)
 */

#ifndef __GT5663_H
#define __GT5663_H

#include "main.h"

/* 硬件引脚 (直接寄存器访问, 不依赖位带) */
#define GT_RST_L()  (GPIOC->BSRR = (uint32_t)GPIO_PIN_13 << 16U)
#define GT_RST_H()  (GPIOC->BSRR = GPIO_PIN_13)
#define GT_INT_RD() ((GPIOB->IDR & GPIO_PIN_1) != 0)

/* I2C 7-bit 地址: 0x14 → 写 0x28 / 读 0x29 */
#define GT_CMD_WR   0x28
#define GT_CMD_RD   0x29

/* 寄存器 (16-bit 地址) */
#define GT_CTRL_REG     0x8040
#define GT_CFGS_REG     0x8050
#define GT_CHECK_REG    0x8042
#define GT_PID_REG      0x8140
#define GT_GSTID_REG    0x814E
#define GT_TP1_REG      0x8150
#define GT_TP2_REG      0x8158
#define GT_TP3_REG      0x8160
#define GT_TP4_REG      0x8168
#define GT_TP5_REG      0x8170

uint8_t GT5663_WR_Reg(uint16_t reg, uint8_t *buf, uint8_t len);
void     GT5663_RD_Reg(uint16_t reg, uint8_t *buf, uint8_t len);
uint8_t GT5663_Init(void);
uint8_t GT5663_Scan(void);

/* 触摸点数据结构 */
typedef struct {
    uint16_t x[5];
    uint16_t y[5];
    uint8_t  sta;       /* 触摸状态: bit7=按下, bit3:0=点数 */
    uint8_t  points;
} touch_data_t;

extern touch_data_t g_touch;

#endif
