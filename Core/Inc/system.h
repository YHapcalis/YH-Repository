#ifndef _SYSTEM_H_
#define _SYSTEM_H_

/*
 * 注意: 不使用 "main.h" 以避免循环包含 (main.h → ov7670.h → system.h → main.h)
 * 直接包含 HAL 库顶层头文件, 其会间接包含 CMSIS 设备头文件以获取 GPIO 基地址.
 */
#include "stm32f4xx_hal.h"

/*
 * 位带操作宏 (Bit-band operations)
 * 参考 <<CM3 权威指南>> 第87~92页, M4 与 M3 相同
 *
 * PAout(n) — GPIOA 第 n 引脚输出 (写)
 * PAin(n)  — GPIOA 第 n 引脚输入 (读)
 * 同理 PB/PC/PD/PE/PF/PG/PH/PI
 */

#define BITBAND(addr, bitnum)  ((addr & 0xF0000000) + 0x2000000 + ((addr & 0xFFFFF) << 5) + (bitnum << 2))
#define MEM_ADDR(addr)         (*((volatile unsigned long *)(addr)))
#define BIT_ADDR(addr, bitnum) MEM_ADDR(BITBAND(addr, bitnum))

/* GPIO ODR 地址 (输出) */
#define GPIOA_ODR_Addr    (GPIOA_BASE + 20)
#define GPIOB_ODR_Addr    (GPIOB_BASE + 20)
#define GPIOC_ODR_Addr    (GPIOC_BASE + 20)
#define GPIOD_ODR_Addr    (GPIOD_BASE + 20)
#define GPIOE_ODR_Addr    (GPIOE_BASE + 20)
#define GPIOF_ODR_Addr    (GPIOF_BASE + 20)
#define GPIOG_ODR_Addr    (GPIOG_BASE + 20)
#define GPIOH_ODR_Addr    (GPIOH_BASE + 20)
#define GPIOI_ODR_Addr    (GPIOI_BASE + 20)

/* GPIO IDR 地址 (输入) */
#define GPIOA_IDR_Addr    (GPIOA_BASE + 16)
#define GPIOB_IDR_Addr    (GPIOB_BASE + 16)
#define GPIOC_IDR_Addr    (GPIOC_BASE + 16)
#define GPIOD_IDR_Addr    (GPIOD_BASE + 16)
#define GPIOE_IDR_Addr    (GPIOE_BASE + 16)
#define GPIOF_IDR_Addr    (GPIOF_BASE + 16)
#define GPIOG_IDR_Addr    (GPIOG_BASE + 16)
#define GPIOH_IDR_Addr    (GPIOH_BASE + 16)
#define GPIOI_IDR_Addr    (GPIOI_BASE + 16)

/* 位带操作宏 (n 必须小于 16) */
#define PAout(n)  BIT_ADDR(GPIOA_ODR_Addr, n)  /* 输出 */
#define PAin(n)   BIT_ADDR(GPIOA_IDR_Addr, n)  /* 输入 */

#define PBout(n)  BIT_ADDR(GPIOB_ODR_Addr, n)
#define PBin(n)   BIT_ADDR(GPIOB_IDR_Addr, n)

#define PCout(n)  BIT_ADDR(GPIOC_ODR_Addr, n)
#define PCin(n)   BIT_ADDR(GPIOC_IDR_Addr, n)

#define PDout(n)  BIT_ADDR(GPIOD_ODR_Addr, n)
#define PDin(n)   BIT_ADDR(GPIOD_IDR_Addr, n)

#define PEout(n)  BIT_ADDR(GPIOE_ODR_Addr, n)
#define PEin(n)   BIT_ADDR(GPIOE_IDR_Addr, n)

#define PFout(n)  BIT_ADDR(GPIOF_ODR_Addr, n)
#define PFin(n)   BIT_ADDR(GPIOF_IDR_Addr, n)

#define PGout(n)  BIT_ADDR(GPIOG_ODR_Addr, n)
#define PGin(n)   BIT_ADDR(GPIOG_IDR_Addr, n)

#define PHout(n)  BIT_ADDR(GPIOH_ODR_Addr, n)
#define PHin(n)   BIT_ADDR(GPIOH_IDR_Addr, n)

#define PIout(n)  BIT_ADDR(GPIOI_ODR_Addr, n)
#define PIin(n)   BIT_ADDR(GPIOI_IDR_Addr, n)

/* 类型定义 (兼容官方例程) */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#endif /* _SYSTEM_H_ */
