/**
 * @file    ctiic.h
 * @brief   GT5663/GT5688 触摸专用软件 I2C (PB0=SCL, PF11=SDA)
 */

#ifndef __CTIIC_H
#define __CTIIC_H

#include "main.h"

/* ---- 位带操作 (GPIO 快速位访问) ---- */
#define BITBAND_PERI(addr, bitnum)  (*(__IO uint32_t *)(0x42000000U + (((uint32_t)(addr) & 0x000FFFFFU) * 32U) + ((bitnum) * 4U)))
#define PBout(n)   BITBAND_PERI(&(GPIOB->ODR), n)
#define PBin(n)    BITBAND_PERI(&(GPIOB->IDR), n)
#define PCout(n)   BITBAND_PERI(&(GPIOC->ODR), n)
#define PFout(n)   BITBAND_PERI(&(GPIOF->ODR), n)
#define PFin(n)    BITBAND_PERI(&(GPIOF->IDR), n)

/* 触摸专用 I2C 引脚 */
#define CT_IIC_SCL    PBout(0)
#define CT_IIC_SDA    PFout(11)
#define CT_READ_SDA   PFin(11)

void CT_IIC_Init(void);
void CT_IIC_Start(void);
void CT_IIC_Stop(void);
void CT_IIC_Send_Byte(uint8_t txd);
uint8_t CT_IIC_Read_Byte(unsigned char ack);
uint8_t CT_IIC_Wait_Ack(void);
void CT_IIC_Ack(void);
void CT_IIC_NAck(void);
void CT_Delay(void);

#endif
