#include "sccb.h"
#include "delay.h"   /* DWT us delay */

/*
 * SCCB 总线驱动 (GPIO 模拟)
 *
 * 时序要求:
 *   tLOW  = 最少 1.3us (SCCB 标准模式)
 *   实际使用 ~50us 确保兼容性
 *
 * SDA 方向切换:
 *   写周期: SDA 为推挽输出
 *   应答位: SDA 切换为输入, 由从机驱动
 */

static void SCCB_SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

static void SCCB_SDA_IN(void)
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

    /* PD7 (SDA) — 初始为输入 (高阻) */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* PD6 (SCL) — 推挽输出, 初始高电平 */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_6, GPIO_PIN_SET);

    SCCB_SDA_OUT();
}

void SCCB_Start(void)
{
    SCCB_SDA = 1;
    SCCB_SCL = 1;
    delay_us(50);
    SCCB_SDA = 0;
    delay_us(50);
    SCCB_SCL = 0;
}

void SCCB_Stop(void)
{
    SCCB_SDA = 0;
    delay_us(50);
    SCCB_SCL = 1;
    delay_us(50);
    SCCB_SDA = 1;
    delay_us(50);
}

void SCCB_No_Ack(void)
{
    delay_us(50);
    SCCB_SDA = 1;
    SCCB_SCL = 1;
    delay_us(50);
    SCCB_SCL = 0;
    delay_us(50);
    SCCB_SDA = 0;
    delay_us(50);
}

u8 SCCB_WR_Byte(u8 dat)
{
    u8 j, res;

    for (j = 0; j < 8; j++) {
        if (dat & 0x80)
            SCCB_SDA = 1;
        else
            SCCB_SDA = 0;
        dat <<= 1;
        delay_us(50);
        SCCB_SCL = 1;
        delay_us(50);
        SCCB_SCL = 0;
    }

    /* 读取应答 */
    SCCB_SDA_IN();
    delay_us(50);
    SCCB_SCL = 1;
    delay_us(50);
    if (SCCB_READ_SDA)
        res = 1;    /* NACK */
    else
        res = 0;    /* ACK */
    SCCB_SCL = 0;
    SCCB_SDA_OUT();

    return res;
}

u8 SCCB_RD_Byte(void)
{
    u8 temp = 0, j;

    SCCB_SDA_IN();
    for (j = 8; j > 0; j--) {
        delay_us(50);
        SCCB_SCL = 1;
        temp <<= 1;
        if (SCCB_READ_SDA) temp++;
        delay_us(50);
        SCCB_SCL = 0;
    }
    SCCB_SDA_OUT();

    return temp;
}

u8 SCCB_WR_Reg(u8 reg, u8 data)
{
    u8 res = 0;

    SCCB_Start();
    if (SCCB_WR_Byte(SCCB_ID))     res = 1;  /* 设备地址 */
    delay_us(100);
    if (SCCB_WR_Byte(reg))         res = 1;  /* 寄存器地址 */
    delay_us(100);
    if (SCCB_WR_Byte(data))        res = 1;  /* 写入数据 */
    SCCB_Stop();

    return res;
}

u8 SCCB_RD_Reg(u8 reg)
{
    u8 val = 0;

    SCCB_Start();
    SCCB_WR_Byte(SCCB_ID);          /* 写方向: 设备地址 */
    delay_us(100);
    SCCB_WR_Byte(reg);              /* 寄存器地址 */
    delay_us(100);
    SCCB_Stop();
    delay_us(100);

    /* 重新开始, 读方向 */
    SCCB_Start();
    SCCB_WR_Byte(SCCB_ID | 0x01);   /* 读方向: 设备地址 | 0x01 */
    delay_us(100);
    val = SCCB_RD_Byte();           /* 读取数据 */
    SCCB_No_Ack();
    SCCB_Stop();

    return val;
}
