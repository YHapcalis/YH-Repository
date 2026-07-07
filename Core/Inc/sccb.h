#ifndef _SCCB_H_
#define _SCCB_H_

#include "system.h"

/* SCCB 总线引脚 (GPIO 模拟) */
#define SCCB_SCL        PDout(6)  /* SCL — PD6 */
#define SCCB_SDA        PDout(7)  /* SDA — PD7 (输出时) */
#define SCCB_READ_SDA   PDin(7)   /* SDA — PD7 (输入时) */

/* OV7670 SCCB 设备地址 (7-bit: 0x21, 左移1位: 0x42) */
#define SCCB_ID         0x42

/* 函数声明 */
void SCCB_Init(void);
void SCCB_Start(void);
void SCCB_Stop(void);
void SCCB_No_Ack(void);
u8   SCCB_WR_Byte(u8 dat);
u8   SCCB_RD_Byte(void);
u8   SCCB_WR_Reg(u8 reg, u8 data);
u8   SCCB_RD_Reg(u8 reg);

#endif /* _SCCB_H_ */
