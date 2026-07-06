#ifndef _OV7670_H
#define _OV7670_H

#include "system.h"
#include "gpio.h"

/* 引脚定义 */
#define OV7670_VSYNC    PAin(8)
#define OV7670_WRST     PBout(7)
#define OV7670_WREN     PGout(9)
#define OV7670_RCK_H    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET)
#define OV7670_RCK_L    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET)
#define OV7670_RRST     PAout(4)
#define OV7670_CS       PGout(15)

#define OV7670_DATA  ((PEin(6)<<7)|(PEin(5)<<6)|(PBin(6)<<5)|(PCin(11)<<4)|(PCin(9)<<3)|(PCin(8)<<2)|(PCin(7)<<1)|(PCin(6)<<0))

extern u8 ov_sta;

u8  OV7670_Init(void);
void OV7670_Light_Mode(u8 mode);
void OV7670_Color_Saturation(u8 sat);
void OV7670_Brightness(u8 bright);
void OV7670_Contrast(u8 contrast);
void OV7670_Special_Effects(u8 eft);
void OV7670_Window_Set(u16 sx, u16 sy, u16 width, u16 height);
void camera_refresh(void);

#endif
