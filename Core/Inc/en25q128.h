/**
 * @file    en25q128.h
 * @brief   W25Q128JV SPI Flash 寄存器级驱动 (SPI1: PB3/SCK, PB4/MISO, PB5/MOSI, PB14/CS)
 */

#ifndef __EN25Q128_H
#define __EN25Q128_H

#include "main.h"

/* ---- 芯片常量 ---- */
#define EN25Q128_PAGE_SIZE          256U
#define EN25Q128_SECTOR_SIZE        4096U
#define EN25Q128_EXPECTED_ID        0xEF6018U   /* Winbond W25Q128JV */

/* ---- 命令 ---- */
#define EN25Q128_CMD_WREN           0x06U       /* 写使能               */
#define EN25Q128_CMD_WRDI           0x04U       /* 写禁止               */
#define EN25Q128_CMD_RDSR1          0x05U       /* 读状态寄存器 1        */
#define EN25Q128_CMD_RDSR2          0x35U       /* 读状态寄存器 2        */
#define EN25Q128_CMD_READ           0x03U       /* 读数据               */
#define EN25Q128_CMD_PP             0x02U       /* 页编程 (≤256B)       */
#define EN25Q128_CMD_SE             0x20U       /* 扇区擦除 (4KB)       */
#define EN25Q128_CMD_RDID           0x9FU       /* 读 JEDEC ID (3 字节) */

/* ---- SR1 位 ---- */
#define EN25Q128_SR1_BUSY           0x01U

/* ---- 初始化 BR 值 ---- */
#define EN25Q128_BR_SAFE            7U          /* fPCLK/256 = 328KHz  */
#define EN25Q128_BR_FAST            2U          /* fPCLK/8   = 10.5MHz */

/* ---- API ---- */
void     EN25Q128_Init(void);
uint32_t EN25Q128_ReadID(void);
uint8_t  EN25Q128_ReadSR1(void);
uint8_t  EN25Q128_ReadSR2(void);
void     EN25Q128_WriteEnable(void);
void     EN25Q128_WaitBusy(void);
void     EN25Q128_EraseSector(uint32_t addr);
void     EN25Q128_Write(const uint8_t *buf, uint32_t addr, uint32_t len);
void     EN25Q128_Read(uint8_t *buf, uint32_t addr, uint32_t len);
void     EN25Q128_SetSpeed(uint8_t br);

#endif /* __EN25Q128_H */
