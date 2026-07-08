/**
 * @file    en25q128.c
 * @brief   W25Q128JV SPI Flash 驱动 (DRIVER 层)
 *
 * 硬件连接: SPI1 @ PB3(SCK) PB4(MISO) PB5(MOSI) PB14(CS)
 * 通信协议: SPI Mode 3 (CPOL=1, CPHA=1), MSB first
 *
 * 关键命令表:
 *   0x03  读数据         1+3+N 字节, 无页限制
 *   0x02  页编程(写)     1+3+N 字节, 每页 ≤256 字节
 *   0x20  扇区擦除(4KB)  1+3 字节, ~45ms
 *   0xD8  块擦除(64KB)   1+3 字节, ~1s
 *   0x06  写使能
 *   0x9F  读 JEDEC ID    1+3 字节 → 0xEF6018 (Winbond W25Q128JV)
 *
 * 注意: SPI GPIO 速度用 MEDIUM(25MHz) 而非 VERY_HIGH(100MHz),
 *       后者会引起过冲/串扰导致通信出错！
 *
 * ============================================================
 * 本文件同时承担了两层职责:
 *   DRIVER:  EN25Q128_Read/Write/Erase — 裸 SPI Flash 操作
 *   SERVICE: EN25Q128_BackupFirmware   — 固件备份/恢复
 * ============================================================
 */

#include "en25q128.h"
#include "inter_flashif.h"
#include "lfs_port.h"
#include <stdio.h>
#include <string.h>

/* ---- 内部辅助宏 ---- */
#define CS_LOW()    do { GPIOB->BSRR = (uint32_t)FLASH_CS_Pin << 16U; } while(0)
#define CS_HIGH()   do { GPIOB->BSRR = FLASH_CS_Pin;              } while(0)

static uint8_t g_br = EN25Q128_BR_SAFE; /* 当前分频系数 */

/* ---- SPI1 单字节收发 (等待模式, 阻塞直到完成) ---- */
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

/* ---- 读数据 (不关中断 — 由调用者统一保护) ---- */
void EN25Q128_Read(volatile uint8_t *buf, uint32_t addr, uint32_t len)
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

/* ---- 擦除后写入 (NOR Flash 必须先擦除再写) ---- */
void EN25Q128_EraseWrite(const uint8_t *buf, uint32_t addr, uint32_t len)
{
    uint32_t end = addr + len;
    uint32_t sec_start = addr & ~(EN25Q128_SECTOR_SIZE - 1);
    uint32_t sec_end   = (end + EN25Q128_SECTOR_SIZE - 1) & ~(EN25Q128_SECTOR_SIZE - 1);

    __disable_irq();                    /* 防 TIM3 中断破坏 CS 时序 */
    for (uint32_t s = sec_start; s < sec_end; s += EN25Q128_SECTOR_SIZE) {
        EN25Q128_EraseSector(s);
    }
    EN25Q128_Write(buf, addr, len);
    __enable_irq();
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

/* ═══════════════════════════════════════════════════════════
 *  SPI Flash 固件备份 / 恢复
 * ═══════════════════════════════════════════════════════════ */

extern uint32_t _fw_end;
#define APP_BASE        ((uint32_t)0x08010000UL)
#define FW_MAX_SIZE     ((uint32_t)(512UL * 1024UL))

static uint8_t g_block_buf[2048];              /* 恢复用块缓冲（栈上 2KB 太危险） */

/* ---- 备份进度回调（可选，APP 设置后用于实时更新屏幕） ---- */
static backup_progress_t s_bak_cb = NULL;

void EN25Q128_SetBackupProgressCb(backup_progress_t cb)
{
    s_bak_cb = cb;
}

uint8_t EN25Q128_BackupFirmware(void)
{
    uint32_t fw_size = (uint32_t)&_fw_end - APP_BASE;
    if (fw_size > FW_MAX_SIZE) fw_size = FW_MAX_SIZE;
    printf("[SPI] Backing up %lu bytes...\r\n", fw_size);

    uint32_t total_steps = 128 + 1 + (fw_size + EN25Q128_PAGE_SIZE - 1) / EN25Q128_PAGE_SIZE;
    uint32_t step = 0;

    /* 擦除备份区域 128 个 sector (4KB × 128 = 512KB) */
    uint32_t addr = SPI_BAK_ADDR;
    for (uint32_t i = 0; i < 128; i++) {
        EN25Q128_EraseSector(addr);
        addr += EN25Q128_SECTOR_SIZE;
        step++;
        if (s_bak_cb) s_bak_cb(step, total_steps, "正在擦除...");
    }

    /* 写入 8 字节备份头 */
    uint8_t hdr[8];
    hdr[0] = (uint8_t)(SPI_BAK_MAGIC >> 0);
    hdr[1] = (uint8_t)(SPI_BAK_MAGIC >> 8);
    hdr[2] = (uint8_t)(SPI_BAK_MAGIC >> 16);
    hdr[3] = (uint8_t)(SPI_BAK_MAGIC >> 24);
    hdr[4] = (uint8_t)(fw_size >> 0);
    hdr[5] = (uint8_t)(fw_size >> 8);
    hdr[6] = (uint8_t)(fw_size >> 16);
    hdr[7] = (uint8_t)(fw_size >> 24);
    EN25Q128_EraseWrite(hdr, SPI_BAK_ADDR, 8);
    step++;
    if (s_bak_cb) s_bak_cb(step, total_steps, "正在写入...");

    /* 分页写入固件数据 */
    uint8_t page[EN25Q128_PAGE_SIZE];
    uint32_t spi_off = SPI_BAK_ADDR + 8;
    for (uint32_t off = 0; off < fw_size; off += EN25Q128_PAGE_SIZE) {
        uint32_t n = fw_size - off;
        if (n > EN25Q128_PAGE_SIZE) n = EN25Q128_PAGE_SIZE;
        memcpy(page, (const void *)(APP_BASE + off), n);
        if (n < EN25Q128_PAGE_SIZE)
            memset(page + n, 0xFF, EN25Q128_PAGE_SIZE - n);
        EN25Q128_Write(page, spi_off, EN25Q128_PAGE_SIZE);
        spi_off += EN25Q128_PAGE_SIZE;
        step++;
        if (s_bak_cb && (step & 0x1F) == 0)
            s_bak_cb(step, total_steps, "正在写入...");
    }
    printf("[SPI] Backup done (%lu bytes)\r\n", fw_size);
    return 0;
}

uint8_t EN25Q128_RestoreFirmware(void)
{
    uint8_t hdr[8];
    EN25Q128_Read(hdr, SPI_BAK_ADDR, 8);
    uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
                     ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    if (magic != SPI_BAK_MAGIC) {
        printf("[SPI] No valid backup (magic=0x%08lX)\r\n", magic);
        return 1;
    }
    uint32_t fw_size = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) |
                       ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    if (fw_size > FW_MAX_SIZE) return 2;
    printf("[SPI] Restoring %lu bytes...\r\n", fw_size);

    uint32_t addr = APP_BASE;
    while (addr < APP_BASE + fw_size) {
        if (addr < 0x08010000)      { inter_flashif_erase_sector(addr); addr += 0x4000; }
        else if (addr < 0x08020000) { inter_flashif_erase_sector(addr); addr += 0x10000; }
        else                        { inter_flashif_erase_sector(addr); addr += 0x20000; }
    }

#define RB 2048
    uint32_t spi_off = SPI_BAK_ADDR + 8;
    for (uint32_t off = 0; off < fw_size; off += RB) {
        uint32_t n = fw_size - off;
        if (n > RB) n = RB;
        EN25Q128_Read(g_block_buf, spi_off, n);
        uint32_t words = n / 4;
        if (words > 0) inter_flashif_write(APP_BASE + off, (uint32_t *)g_block_buf, words);
        uint32_t rem = n % 4;
        if (rem > 0) {
            uint32_t last = 0xFFFFFFFF;
            memcpy(&last, g_block_buf + words * 4, rem);
            inter_flashif_write(APP_BASE + off + words * 4, &last, 1);
        }
        spi_off += n;
    }
    printf("[SPI] Restore done\r\n");
    return 0;
}

/* ═════════════════════════════════════════════════════════════
 *  LFS 文件备份 / 恢复（首选路径）
 * ═════════════════════════════════════════════════════════════ */

#define BAK_FILENAME    "firmware.bak"

#ifndef LFS_READONLY
uint8_t EN25Q128_BackupFirmwareLFS(void)
{
    uint32_t fw_size = (uint32_t)&_fw_end - APP_BASE;
    if (fw_size > FW_MAX_SIZE) fw_size = FW_MAX_SIZE;
    printf("[LFS_BAK] Backing up %lu bytes -> %s\n", fw_size, BAK_FILENAME);

    lfs_file_t f;
    int err = lfs_file_open(&g_lfs, &f, BAK_FILENAME,
                            LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err) {
        printf("[LFS_BAK] FAIL open: %d\n", err);
        return 1;
    }

    /* 分块写入 */
    uint8_t buf[256];
    uint32_t remain = fw_size;
    uint32_t off = 0;
    while (remain > 0) {
        uint32_t n = (remain > 256) ? 256 : remain;
        memcpy(buf, (const void *)(APP_BASE + off), n);
        lfs_ssize_t written = lfs_file_write(&g_lfs, &f, buf, n);
        if (written != (lfs_ssize_t)n) {
            printf("[LFS_BAK] FAIL write @ %lu: %d\n", off, (int)written);
            lfs_file_close(&g_lfs, &f);
            return 2;
        }
        off += n;
        remain -= n;
    }

    lfs_file_close(&g_lfs, &f);
    printf("[LFS_BAK] Backup done (%lu bytes)\n", fw_size);
    return 0;
}
#endif /* LFS_READONLY */

uint8_t EN25Q128_RestoreFirmwareLFS(void)
{
    /* 先尝试打开文件检查是否存在 */
    lfs_file_t f;
    int err = lfs_file_open(&g_lfs, &f, BAK_FILENAME, LFS_O_RDONLY);
    if (err) {
        printf("[LFS_BAK] No backup file: %d\n", err);
        return 1;
    }

    /* 读文件大小 */
    lfs_size_t fw_size = lfs_file_size(&g_lfs, &f);
    if (fw_size == 0 || fw_size > FW_MAX_SIZE) {
        printf("[LFS_BAK] Invalid size: %lu\n", (unsigned long)fw_size);
        lfs_file_close(&g_lfs, &f);
        return 2;
    }
    printf("[LFS_BAK] Restoring %lu bytes...\n", (unsigned long)fw_size);

    /* 擦除 APP 区 */
    uint32_t addr = APP_BASE;
    while (addr < APP_BASE + fw_size) {
        if (addr < 0x08020000)      { inter_flashif_erase_sector(addr); addr += 0x10000; }
        else if (addr < 0x08040000) { inter_flashif_erase_sector(addr); addr += 0x20000; }
        else                        { inter_flashif_erase_sector(addr); addr += 0x20000; }
    }

    /* 分块读取 + 写入内部 Flash */
    uint8_t buf[2048];
    uint32_t flash_off = 0;
    while (flash_off < fw_size) {
        uint32_t n = fw_size - flash_off;
        if (n > sizeof(buf)) n = sizeof(buf);

        lfs_ssize_t rd = lfs_file_read(&g_lfs, &f, buf, n);
        if (rd <= 0) break;

        uint32_t words = (uint32_t)rd / 4;
        if (words > 0) inter_flashif_write(APP_BASE + flash_off,
                                           (uint32_t *)buf, words);
        uint32_t rem = (uint32_t)rd % 4;
        if (rem > 0) {
            uint32_t last = 0xFFFFFFFF;
            memcpy(&last, buf + words * 4, rem);
            inter_flashif_write(APP_BASE + flash_off + words * 4, &last, 1);
        }
        flash_off += (uint32_t)rd;
    }

    lfs_file_close(&g_lfs, &f);
    printf("[LFS_BAK] Restore done (%lu bytes)\n", (unsigned long)fw_size);
    return 0;
}
