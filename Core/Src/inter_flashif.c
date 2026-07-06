/*
 * inter_flashif.c — 内部 Flash 读写擦（F407 适配版）
 *
 * 与 F1 例程的关键差异:
 *   F407 使用 sector 擦除（最小 16KB），不是 F1 的 page 擦除（1KB）
 *   编程单位使用 FLASH_TYPEPROGRAM_WORD（32-bit），与 F1 兼容
 *
 * 参数扇区: INTER_FLASH_PARAM_ADDR = 0x080E0000 (Sector 11, 128KB)
 * 注意: 擦除以 sector 为单位，即使只改 1 字节也要擦整个 128KB！
 */

#include "inter_flashif.h"
#include "stm32f4xx_hal.h"

/* ═══════════════════════ 累加和校验 ═══════════════════════ */

uint8_t inter_flash_checksum(uint8_t *data, uint32_t len)
{
    uint8_t checksum = 0;
    for (uint32_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    return checksum & 0xFF;
}

/* ═══════════════════════ 读 Flash（直接指针访问） ═══════════════════════ */

uint8_t inter_flashif_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = *(volatile uint8_t *)(addr + i);
    }
    return 0;
}

/* ═══════════════════════ 写 Flash（按字写入） ═══════════════════════ */

uint8_t inter_flashif_write(uint32_t addr, uint32_t *buf, uint32_t word_len)
{
    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < word_len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i * 4, buf[i])
            != HAL_OK) {
            HAL_FLASH_Lock();
            return 1;
        }
    }

    HAL_FLASH_Lock();
    return 0;
}

/* ═══════════════════════ 擦除扇区 ═══════════════════════ */

/*
 * 根据地址自动确定 F407 的 sector 编号:
 *   0x08000000-0x08003FFF: Sector 0  (16KB)
 *   0x08004000-0x08007FFF: Sector 1  (16KB)
 *   0x08008000-0x0800BFFF: Sector 2  (16KB)  ← Bootloader (64KB, Sectors 0-3)
 *   0x0800C000-0x0800FFFF: Sector 3  (16KB)  ← "
 *   0x08010000-0x0801FFFF: Sector 4  (64KB)  ← APP 起始
 *   0x08020000-0x0803FFFF: Sector 5  (128KB)
 *   0x08040000-0x0805FFFF: Sector 6  (128KB)
 *   0x08060000-0x0807FFFF: Sector 7  (128KB)
 *   0x08080000-0x0809FFFF: Sector 8  (128KB)
 *   0x080A0000-0x080BFFFF: Sector 9  (128KB)
 *   0x080C0000-0x080DFFFF: Sector 10 (128KB)
 *   0x080E0000-0x080FFFFF: Sector 11 (128KB)
 */
static uint32_t addr_to_sector(uint32_t addr)
{
    if (addr < 0x08004000) return 0;
    if (addr < 0x08008000) return 1;
    if (addr < 0x0800C000) return 2;
    if (addr < 0x08010000) return 3;
    if (addr < 0x08020000) return 4;
    if (addr < 0x08040000) return 5;
    if (addr < 0x08060000) return 6;
    if (addr < 0x08080000) return 7;
    if (addr < 0x080A0000) return 8;
    if (addr < 0x080C0000) return 9;
    if (addr < 0x080E0000) return 10;
    return 11;
}

uint8_t inter_flashif_erase_sector(uint32_t addr)
{
    FLASH_EraseInitTypeDef erase_init = {0};
    uint32_t sector_error = 0;

    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = addr_to_sector(addr);
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;  /* 2.7V-3.6V */

    HAL_FLASH_Unlock();

    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK) {
        HAL_FLASH_Lock();
        return 1;
    }

    HAL_FLASH_Lock();
    return 0;
}
