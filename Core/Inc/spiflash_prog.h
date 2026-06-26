/**
 * @file    spiflash_prog.h
 * @brief   SPI Flash UART 烧录协议 — MCU 端
 *
 * 协议帧:
 *   PC→MCU: [0xAA][CMD=0x01][4B offset LE][4B size LE][N data][CRC8]
 *   MCU→PC: [0xAA][0x00]=OK / [0xAA][0xFF]=ERR
 */

#ifndef __SPIFLASH_PROG_H
#define __SPIFLASH_PROG_H

#include <stdint.h>

/** SPI Flash 编程模式
 *  监听 UART, 接收并写入 W25Q128
 *  @param timeout_ms  无数据时等待超时 (0=不超时, 一直等)
 *  @return 0=超时退出 (可继续 GUI), 非 0=编程完成帧数
 */
int spiflash_prog_poll(uint32_t timeout_ms);

#endif /* __SPIFLASH_PROG_H */
