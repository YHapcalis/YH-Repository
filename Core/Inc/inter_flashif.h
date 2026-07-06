/*
 * inter_flashif.h — 内部 Flash 读/写/擦除操作（F407 专用）
 *
 * F407 Flash 特性:
 *   - 最小擦除单位: 16KB sector (低 64KB) / 128KB sector (高 960KB)
 *   - 编程单位: 32-bit word (FLASH_TYPEPROGRAM_WORD)
 *   - 参数扇区: Sector 11 @ 0x080E0000, 128KB（仅用前 12 字节）
 */

#ifndef __INTER_FLASHIF_H__
#define __INTER_FLASHIF_H__

#include <stdint.h>

/* ── 参数扇区地址（位于 Flash 最高端，不与其他代码重叠） ── */
#define INTER_FLASH_PARAM_ADDR      (0x080E0000U)   /* Sector 11, 128KB */
#define INTER_FLASH_APP_ADDR        (0x08010000U)   /* APP 起始地址      */

/* ── 接口函数 ── */
uint8_t inter_flashif_read(uint32_t addr, uint8_t *buf, uint32_t len);
uint8_t inter_flashif_write(uint32_t addr, uint32_t *buf, uint32_t word_len);
uint8_t inter_flashif_erase_sector(uint32_t addr);
uint8_t inter_flash_checksum(uint8_t *data, uint32_t len);

#endif /* __INTER_FLASHIF_H__ */
