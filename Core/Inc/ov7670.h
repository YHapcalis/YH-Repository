#ifndef _OV7670_H_
#define _OV7670_H_

#include "system.h"

/*
 * OV7670 + AL422B FIFO 引脚定义
 *
 * 控制引脚说明:
 *   VSYNC  — 帧同步 (输入, EXTI 上升沿触发)
 *   WRST   — FIFO 写指针复位 (低有效)
 *   WREN   — FIFO 写使能 (高有效)
 *   RCK    — FIFO 读时钟 (上升沿锁存)
 *   RRST   — FIFO 读指针复位 (低有效)
 *   CS     — FIFO 输出使能 (低有效)
 */

/* 控制引脚 (位带操作) */
#define OV7670_VSYNC    PAin(8)     /* VSYNC — EXTI 输入 */
#define OV7670_WRST     PBout(7)    /* FIFO 写复位 */
#define OV7670_WREN     PGout(9)    /* FIFO 写使能 */

#define OV7670_RCK_H    (GPIOA->BSRR = GPIO_BSRR_BS6)
#define OV7670_RCK_L    (GPIOA->BSRR = GPIO_BSRR_BR6)

#define OV7670_RRST     PAout(4)    /* FIFO 读复位 */
#define OV7670_CS       PGout(15)   /* FIFO 片选/OE */

/* 8-bit 并行数据输入 (位带读) */
#define OV7670_DATA  ((PEin(6)<<7)|(PEin(5)<<6)|(PBin(6)<<5)|(PCin(11)<<4)|(PCin(9)<<3)|(PCin(8)<<2)|(PCin(7)<<1)|(PCin(6)<<0))

/*
 * 快速数据读取 — 3 次 IDR 寄存器读 + 位组合
 * 替代位带的 16 次读，预计提速 5~7x
 *
 * 位映射:
 *   PE5 → bit6,  PE6 → bit7
 *   PB6 → bit5
 *   PC6~9 → bits0~3,  PC11 → bit4
 *
 * 内联函数: 编译器自动优化为 3 次 LDR + 4 条逻辑指令
 */
__attribute__((always_inline))
static inline uint8_t ov7670_read_byte(void)
{
    uint32_t idr_c = GPIOC->IDR;
    uint32_t idr_b = GPIOB->IDR;
    uint32_t idr_e = GPIOE->IDR;

    return (uint8_t)(((idr_e << 1) & 0xC0) | ((idr_b >> 1) & 0x20) |
                     ((idr_c >> 6) & 0x0F) | ((idr_c >> 7) & 0x10));
}

/*
 * 帧中断标志 ov_sta
 *   ov_sta == 0: 等待帧开始
 *   ov_sta == 1: 帧正在进行 (FIFO 写入中)
 *   ov_sta == 2: 帧已完成, 等待主循环读取
 */
extern volatile u8 ov_sta;     /* 帧中断状态 (在 ov7670.c 定义) */
extern volatile u8 ov_frame;   /* 帧计数器 */

/* DWT 微秒延时 (供 sccb.c 使用) */
void OV7670_delay_us(u32 us);

/* OV7670 驱动函数 */
u8   OV7670_Init(void);
void OV7670_Light_Mode(u8 mode);
void OV7670_Color_Saturation(u8 sat);
void OV7670_Brightness(u8 bright);
void OV7670_Contrast(u8 contrast);
void OV7670_Special_Effects(u8 eft);
void OV7670_Window_Set(u16 sx, u16 sy, u16 width, u16 height);

/* 摄像头刷新 (需要 LCD 驱动配合) */
void camera_refresh(void);

#endif /* _OV7670_H_ */
