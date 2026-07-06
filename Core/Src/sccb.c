/*
 * sccb.c — SCCB 总线驱动 (OV7670 摄像头配置)
 *
 * 使用位带操作 (system.h), 不依赖 STD_PERIPH 库。
 * PD6=SCL, PD7=SDA (与 CubeMX 引脚配置一致)
 */

#include "sccb.h"
#include "system.h"
#include "delay.h"
#include <stdio.h>

static void sccb_delay(void)
{
    delay_us(50);
}

void SCCB_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

void SCCB_SDA_IN(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

void SCCB_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_7, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, GPIO_PIN_SET);

    SCCB_SDA_OUT();
}

void SCCB_Start(void)
{
    SCCB_SDA = 1;
    SCCB_SCL = 1;
    sccb_delay();
    SCCB_SDA = 0;
    sccb_delay();
    SCCB_SCL = 0;
}

void SCCB_Stop(void)
{
    SCCB_SDA = 0;
    sccb_delay();
    SCCB_SCL = 1;
    sccb_delay();
    SCCB_SDA = 1;
    sccb_delay();
}

void SCCB_No_Ack(void)
{
    sccb_delay();
    SCCB_SDA = 1;
    SCCB_SCL = 1;
    sccb_delay();
    SCCB_SCL = 0;
    sccb_delay();
    SCCB_SDA = 0;
    sccb_delay();
}

u8 SCCB_WR_Byte(u8 dat)
{
    u8 j, res;
    for (j = 0; j < 8; j++) {
        SCCB_SDA = (dat & 0x80) ? 1 : 0;
        dat <<= 1;
        sccb_delay();
        SCCB_SCL = 1;
        sccb_delay();
        SCCB_SCL = 0;
    }
    SCCB_SDA_IN();
    sccb_delay();
    SCCB_SCL = 1;
    sccb_delay();
    res = SCCB_READ_SDA;
    SCCB_SCL = 0;
    SCCB_SDA_OUT();
    return res;
}

u8 SCCB_RD_Byte(void)
{
    u8 temp = 0, j;
    SCCB_SDA_IN();
    for (j = 8; j > 0; j--) {
        sccb_delay();
        SCCB_SCL = 1;
        temp <<= 1;
        if (SCCB_READ_SDA) temp++;
        sccb_delay();
        SCCB_SCL = 0;
    }
    SCCB_SDA_OUT();
    return temp;
}

u8 SCCB_WR_Reg(u8 reg, u8 data)
{
    u8 res = 0;
    SCCB_Start();
    if (SCCB_WR_Byte(SCCB_ID)) res = 1;
    sccb_delay();
    if (SCCB_WR_Byte(reg)) res = 1;
    sccb_delay();
    if (SCCB_WR_Byte(data)) res = 1;
    SCCB_Stop();
    return res;
}

u8 SCCB_RD_Reg(u8 reg)
{
    u8 val = 0;
    SCCB_Start();
    SCCB_WR_Byte(SCCB_ID);
    sccb_delay();
    SCCB_WR_Byte(reg);
    sccb_delay();
    SCCB_Stop();
    sccb_delay();
    SCCB_Start();
    SCCB_WR_Byte(SCCB_ID | 0x01);
    sccb_delay();
    val = SCCB_RD_Byte();
    SCCB_No_Ack();
    SCCB_Stop();
    return val;
}
