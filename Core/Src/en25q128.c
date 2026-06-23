/**
 * @file    en25q128.c
 * @brief   W25Q128JV 寄存器级驱动 — SPI1 直接操作，不依赖 HAL SPI 句柄
 *          硬件：PB3=SCK, PB4=MISO, PB5=MOSI, PB14=CS (SPI1-AF5)
 */

#include "en25q128.h"

/* ---- 内部辅助宏 ---- */
#define CS_LOW()    do { GPIOB->BSRR = (uint32_t)FLASH_CS_Pin << 16U; } while(0)
#define CS_HIGH()   do { GPIOB->BSRR = FLASH_CS_Pin;              } while(0)

static uint8_t g_br = EN25Q128_BR_SAFE; /* 当前分频系数 */

/* ---- SPI1 字节交换 ---- */
static uint8_t spi1_xfer(uint8_t tx)
{
    /* 等待 TXE (发送缓冲空) */
    while (!(SPI1->SR & SPI_SR_TXE));
    *(volatile uint8_t *)&SPI1->DR = tx;
    /* 等待 RXNE (接收缓冲非空) */
    while (!(SPI1->SR & SPI_SR_RXNE));
    return *(volatile uint8_t *)&SPI1->DR;
}

/* ---- 写使能 ---- */
void EN25Q128_WriteEnable(void)
{
    CS_LOW();
    spi1_xfer(EN25Q128_CMD_WREN);
    CS_HIGH();
}

/* ---- 等待忙闲 ---- */
void EN25Q128_WaitBusy(void)
{
    CS_LOW();
    spi1_xfer(EN25Q128_CMD_RDSR1);
    while (spi1_xfer(0xFF) & EN25Q128_SR1_BUSY);
    CS_HIGH();
}

/* ---- 读状态寄存器 1 ---- */
uint8_t EN25Q128_ReadSR1(void)
{
    uint8_t sr;
    CS_LOW();
    spi1_xfer(EN25Q128_CMD_RDSR1);
    sr = spi1_xfer(0xFF);
    CS_HIGH();
    return sr;
}

/* ---- 读状态寄存器 2 ---- */
uint8_t EN25Q128_ReadSR2(void)
{
    uint8_t sr;
    CS_LOW();
    spi1_xfer(EN25Q128_CMD_RDSR2);
    sr = spi1_xfer(0xFF);
    CS_HIGH();
    return sr;
}

/* ---- 读 JEDEC ID (3 字节) ---- */
uint32_t EN25Q128_ReadID(void)
{
    uint32_t id = 0;
    CS_LOW();
    spi1_xfer(EN25Q128_CMD_RDID);
    id  = (uint32_t)spi1_xfer(0xFF) << 16;
    id |= (uint32_t)spi1_xfer(0xFF) << 8;
    id |= (uint32_t)spi1_xfer(0xFF);
    CS_HIGH();
    return id;
}

/* ---- 扇区擦除 (4KB) ---- */
void EN25Q128_EraseSector(uint32_t addr)
{
    EN25Q128_WriteEnable();
    CS_LOW();
    spi1_xfer(EN25Q128_CMD_SE);
    spi1_xfer((uint8_t)(addr >> 16));
    spi1_xfer((uint8_t)(addr >> 8));
    spi1_xfer((uint8_t)(addr));
    CS_HIGH();
    EN25Q128_WaitBusy();
}

/* ---- 页写入 (调用者保证不跨页 + addr 已擦除为 0xFF) ---- */
static void page_write(const uint8_t *buf, uint32_t addr, uint32_t len)
{
    EN25Q128_WriteEnable();
    CS_LOW();
    spi1_xfer(EN25Q128_CMD_PP);
    spi1_xfer((uint8_t)(addr >> 16));
    spi1_xfer((uint8_t)(addr >> 8));
    spi1_xfer((uint8_t)(addr));
    while (len--) {
        spi1_xfer(*buf++);
    }
    CS_HIGH();
    EN25Q128_WaitBusy();
}

/* ---- 写数据 (自动跨页，不自动擦除) ---- */
void EN25Q128_Write(const uint8_t *buf, uint32_t addr, uint32_t len)
{
    while (len > 0) {
        uint32_t page_off  = addr & (EN25Q128_PAGE_SIZE - 1);
        uint32_t space     = EN25Q128_PAGE_SIZE - page_off;
        uint32_t chunk     = (len < space) ? len : space;
        page_write(buf, addr, chunk);
        addr += chunk;
        buf  += chunk;
        len  -= chunk;
    }
}

/* ---- 读数据 ---- */
void EN25Q128_Read(uint8_t *buf, uint32_t addr, uint32_t len)
{
    CS_LOW();
    spi1_xfer(EN25Q128_CMD_READ);
    spi1_xfer((uint8_t)(addr >> 16));
    spi1_xfer((uint8_t)(addr >> 8));
    spi1_xfer((uint8_t)(addr));
    while (len--) {
        *buf++ = spi1_xfer(0xFF);
    }
    CS_HIGH();
}

/* ---- 设置 SPI 速率 ---- */
void EN25Q128_SetSpeed(uint8_t br)
{
    g_br = br & 0x07;
    SPI1->CR1 = (SPI1->CR1 & ~SPI_CR1_BR) | (g_br << 3);
}

/* ---- 初始化 ---- */
void EN25Q128_Init(void)
{
    /* PB14 CS 初始 HIGH (片选无效) */
    HAL_GPIO_WritePin(FLASH_CS_GPIO_Port, FLASH_CS_Pin, GPIO_PIN_SET);

    /* 禁用 SPI1 再重配 */
    SPI1->CR1 &= ~SPI_CR1_SPE;

    /* CR1: Master, Mode3 (CPOL=1,CPHA=1), MSB, SSM=1, SSI=1, BR=SAFE */
    SPI1->CR1 = SPI_CR1_MSTR
              | SPI_CR1_CPOL
              | SPI_CR1_CPHA
              | SPI_CR1_SSM
              | SPI_CR1_SSI
              | (EN25Q128_BR_SAFE << 3);  /* BR = fPCLK/256 */

    /* CR2: 8-bit data, NSS pulse disabled */
    SPI1->CR2 = 0;

    /* 使能 SPI1 */
    SPI1->CR1 |= SPI_CR1_SPE;

    g_br = EN25Q128_BR_SAFE;

    /* 空读一次清 RX 缓冲 */
    (void)spi1_xfer(0xFF);
}
