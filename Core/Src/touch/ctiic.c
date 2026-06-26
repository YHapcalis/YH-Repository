/**
 * @file    ctiic.c
 * @brief   触摸专用软件 I2C (PB0=SCL, PF11=SDA)
 *          v3: 严格对齐官方金星例程 ct iic.c (APP/touch/ctiic.c)
 *          关键修复:
 *          1. CT_IIC_Wait_Ack 切换输入后先释放 SDA (写 ODR=1) 再发第9个时钟
 *          2. CT_IIC_Read_Byte 切换输入后加 30us 稳定延时
 *          3. CT_IIC_Send_Byte 先拉 SCL 再进循环 (与官方完全一致)
 *          4. CT_IIC_Stop 官方序列: SCL=1 → delay → SDA=0 → delay → SDA=1
 */

#include "ctiic.h"

/* ---- us 延时 (DWT 周期计数器) ---- */
static void delay_us(uint32_t nus)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = nus * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks);
}

/* ---- SDA 方向切换 ---- */
static void CT_SDA_OUT(void)
{
    GPIOF->MODER = (GPIOF->MODER & ~(3U << 22)) | (1U << 22);  /* PF11 → OUTPUT */
}

static void CT_SDA_IN(void)
{
    GPIOF->MODER = (GPIOF->MODER & ~(3U << 22)) | (0U << 22);  /* PF11 → INPUT */
}

/* ---- SCL / SDA 电平控制 (BSRR: 原子操作) ---- */
#define SCL_H()   (GPIOB->BSRR = GPIO_PIN_0)
#define SCL_L()   (GPIOB->BSRR = (uint32_t)GPIO_PIN_0 << 16U)
#define SDA_H()   (GPIOF->BSRR = GPIO_PIN_11)
#define SDA_L()   (GPIOF->BSRR = (uint32_t)GPIO_PIN_11 << 16U)
#define SDA_RD()  ((GPIOF->IDR & GPIO_PIN_11) != 0)

/* ---- 初始上拉 (PUPDR) ---- */
static void CT_PUPD_SDA(uint8_t pull)
{
    GPIOF->PUPDR = (GPIOF->PUPDR & ~(3U << 22)) | ((uint32_t)pull << 22);
}
static void CT_PUPD_SCL(uint8_t pull)
{
    GPIOB->PUPDR = (GPIOB->PUPDR & ~(3U << 0)) | ((uint32_t)pull << 0);
}

void CT_Delay(void)
{
    delay_us(5);  /* 官方 5us (~100kHz I2C), 已验证稳定 */
}

void CT_IIC_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* PB0 = SCL: 推挽输出 + 上拉 (对齐官方: OUT + PP + 100MHz + PU) */
    GPIOB->MODER   = (GPIOB->MODER & ~(3U << 0)) | (1U << 0);    /* OUTPUT */
    GPIOB->OTYPER &= ~(1U << 0);                                   /* Push-Pull */
    GPIOB->OSPEEDR = (GPIOB->OSPEEDR & ~(3U << 0)) | (3U << 0);  /* Very High (100MHz) */
    CT_PUPD_SCL(1);                                                 /* Pull-up */
    SCL_H();

    /* PF11 = SDA: 推挽输出 + 上拉 (对齐官方: OUT + PP + 100MHz + PU) */
    GPIOF->MODER   = (GPIOF->MODER & ~(3U << 22)) | (1U << 22);   /* OUTPUT */
    GPIOF->OTYPER &= ~(1U << 11);                                   /* Push-Pull */
    GPIOF->OSPEEDR = (GPIOF->OSPEEDR & ~(3U << 22)) | (3U << 22); /* Very High (100MHz) */
    CT_PUPD_SDA(1);                                                  /* Pull-up */
    SDA_H();
}

/* ---- START: SCL=H, SDA 1→0 ---- */
void CT_IIC_Start(void)
{
    CT_SDA_OUT();
    SDA_H();
    SCL_H();
    delay_us(30);
    SDA_L();       /* START: SCL high 时 SDA 从高到低 */
    CT_Delay();
    SCL_L();       /* 钳住总线 */
}

/* ---- STOP: SCL=H, SDA 0→1 (完全对齐官方序列) ---- */
void CT_IIC_Stop(void)
{
    CT_SDA_OUT();
    SCL_H();       /* 先拉高 SCL */
    delay_us(30);
    SDA_L();       /* SDA 拉低 */
    CT_Delay();
    SDA_H();       /* SDA 从低到高 → STOP */
}

/* ---- 等待 ACK: 切换输入 → 释放总线 → 第9个时钟 ---- */
uint8_t CT_IIC_Wait_Ack(void)
{
    uint8_t err = 0;
    CT_SDA_IN();
    SDA_H();       /* ★ 关键: 释放 SDA 总线 (对齐官方 CT_IIC_SDA=1) */
    SCL_H();       /* 第 9 个时钟脉冲 */
    CT_Delay();
    while (SDA_RD()) {
        if (++err > 100) { CT_IIC_Stop(); return 1; }
        CT_Delay();
    }
    SCL_L();
    return 0;
}

void CT_IIC_Ack(void)
{
    SCL_L();
    CT_SDA_OUT();
    SDA_L();
    CT_Delay();
    SCL_H();
    CT_Delay();
    SCL_L();
}

void CT_IIC_NAck(void)
{
    SCL_L();
    CT_SDA_OUT();
    SDA_H();
    CT_Delay();
    SCL_H();
    CT_Delay();
    SCL_L();
}

/* ---- 发送一个字节 (完全对齐官方结构) ---- */
void CT_IIC_Send_Byte(uint8_t txd)
{
    CT_SDA_OUT();
    SCL_L();       /* 先拉低时钟 (对齐官方: 循环前拉低) */
    CT_Delay();
    for (int t = 0; t < 8; t++) {
        if (txd & 0x80) SDA_H(); else SDA_L();
        txd <<= 1;
        SCL_H();   /* 时钟上升沿锁存数据 */
        CT_Delay();
        SCL_L();   /* 时钟拉低准备下一位 */
        CT_Delay();
    }
}

/* ---- 读取一个字节 ---- */
uint8_t CT_IIC_Read_Byte(unsigned char ack)
{
    uint8_t recv = 0;
    CT_SDA_IN();
    delay_us(30);  /* ★ 关键: 切换输入后稳定延时 (对齐官方) */
    for (int i = 0; i < 8; i++) {
        SCL_L();
        CT_Delay();
        SCL_H();
        recv <<= 1;
        if (SDA_RD()) recv++;
        CT_Delay();
    }
    SCL_L();
    if (!ack) CT_IIC_NAck();
    else      CT_IIC_Ack();
    return recv;
}
