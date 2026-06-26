/**
 * @file    spiflash_prog.c
 * @brief   SPI Flash UART 烧录 — 分块握手协议
 *
 * 协议:
 *   Phase 1 — 握手
 *     PC→MCU: [0xAA][CMD=0x01][4B offset LE][4B total_size LE][CRC8]
 *     MCU→PC: [0xAA][0x00]=OK  /  [0xAA][0xFF]=ERR
 *
 *   Phase 2 — 数据块 (重复直到 total_size)
 *     PC→MCU: [2B chunk_size LE][chunk_data][CRC8]
 *     MCU→PC: [0xAA][0x00]=OK  /  [0xAA][0xFF]=ERR
 *
 *   Phase 3 — 结束
 *     PC→MCU: [2B 0x0000][CRC8 over 2B zero]
 *     MCU→PC: [0xAA][0x00]=DONE
 *
 * 关键: MCU 写 SPI Flash 时暂停接收, 写完再 ACK → PC 才发下一块
 */

#include "spiflash_prog.h"
#include "en25q128.h"
#include "usart.h"
#include <string.h>

#define CHUNK_BUF_SIZE  2048

/* ---- CRC8 增量 ---- */
static uint8_t crc8_update(uint8_t crc, const uint8_t *data, uint32_t len)
{
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else            crc = (crc << 1);
        }
    }
    return crc;
}

/* ---- 超时收单字节 ---- */
static int uart_rx_byte_timeout(uint8_t *b, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout_ms) {
        if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
            *b = (uint8_t)(huart1.Instance->DR & 0xFF);
            return 1;
        }
    }
    return 0;
}

/* ---- 发送 ACK/NAK ---- */
static void send_ack(uint8_t ok)
{
    uint8_t ack[2] = {0xAA, ok ? 0x00 : 0xFF};
    HAL_UART_Transmit(&huart1, ack, 2, HAL_MAX_DELAY);
}

/* ---- 可靠接收 N 字节 (无 SPI Flash 写入期间) ---- */
static int uart_rx_bytes(uint8_t *buf, uint32_t len, uint32_t timeout_per_byte)
{
    for (uint32_t i = 0; i < len; i++) {
        if (!uart_rx_byte_timeout(&buf[i], timeout_per_byte)) return 0;
    }
    return 1;
}

/* ---- SPI Flash 编程轮询 (一帧) ---- */
int spiflash_prog_poll(uint32_t timeout_ms)
{
    static uint8_t chunk_buf[CHUNK_BUF_SIZE];
    uint8_t sync, cmd;
    uint32_t offset, total_size, received;
    uint8_t crc_val, crc_rx;

    /* === Phase 1: 同步 + 握手 === */
    if (!uart_rx_byte_timeout(&sync, timeout_ms)) return 0;
    if (sync != 0xAA) return 0;
    if (!uart_rx_byte_timeout(&cmd, 500))  { send_ack(0); return 0; }
    if (cmd != 0x01)                       { send_ack(0); return 0; }

    /* 收 offset (4B LE) */
    {
        uint8_t hdr[8]; /* offset[4] + total_size[4] */
        if (!uart_rx_bytes(hdr, 8, 500))  { send_ack(0); return 0; }
        offset     = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8)
                   | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
        total_size = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8)
                   | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    }

    if (total_size == 0 || total_size > 0x1000000) { send_ack(0); return 0; }

    /* 验证 header CRC: [cmd][offset 4B][total_size 4B] = 9 bytes */
    {
        uint8_t h9[9];
        h9[0] = cmd;
        memcpy(&h9[1], &offset, 4);
        memcpy(&h9[5], &total_size, 4);
        crc_val = crc8_update(0, h9, 9);
    }
    if (!uart_rx_byte_timeout(&crc_rx, 500)) { send_ack(0); return 0; }
    if (crc_rx != crc_val)                     { send_ack(0); return 0; }

    send_ack(1); /* 握手成功 */

    /* === 预先擦除全部扇区 === */
    {
        uint32_t sec_first = offset & ~(EN25Q128_SECTOR_SIZE - 1);
        uint32_t sec_last  = (offset + total_size + EN25Q128_SECTOR_SIZE - 1)
                            & ~(EN25Q128_SECTOR_SIZE - 1);
        for (uint32_t s = sec_first; s < sec_last; s += EN25Q128_SECTOR_SIZE) {
            __disable_irq();
            EN25Q128_EraseSector(s);
            __enable_irq();
        }
    }

    /* === Phase 2: 分块接收 + 写入 === */
    received = 0;
    while (received < total_size) {
        /* 收块头: 2B chunk_size LE */
        uint8_t csz[2];
        if (!uart_rx_bytes(csz, 2, 5000)) { send_ack(0); return -1; }
        uint32_t chunk = (uint32_t)csz[0] | ((uint32_t)csz[1] << 8);

        /* 结束标记? */
        if (chunk == 0) {
            /* 收块 CRC */
            uint8_t end_crc[2] = {0, 0};
            crc_val = crc8_update(0, end_crc, 2);
            if (!uart_rx_byte_timeout(&crc_rx, 500)) { send_ack(0); return -1; }
            send_ack(crc_rx == crc_val ? 1 : 0);
            return (crc_rx == crc_val) ? 1 : -1;
        }

        if (chunk > CHUNK_BUF_SIZE) { send_ack(0); return -1; }

        /* 收块数据 */
        if (!uart_rx_bytes(chunk_buf, chunk, 2000)) { send_ack(0); return -1; }

        /* 收块 CRC */
        crc_val = crc8_update(0, chunk_buf, chunk);
        if (!uart_rx_byte_timeout(&crc_rx, 500)) { send_ack(0); return -1; }
        if (crc_rx != crc_val)                    { send_ack(0); return -1; }

        /* 写入 SPI Flash (此时 UART 不再收数据 — PC 在等 ACK) */
        __disable_irq();
        EN25Q128_Write(chunk_buf, offset + received, chunk);
        __enable_irq();

        received += chunk;
        send_ack(1); /* ACK → PC 发下一块 */
    }

    /* Phase 3: 等待 PC 发结束标记 */
    {
        uint8_t csz[2];
        if (!uart_rx_bytes(csz, 2, 5000)) return -1;
        uint32_t end = (uint32_t)csz[0] | ((uint32_t)csz[1] << 8);
        uint8_t end_crc[2] = {0, 0};
        crc_val = crc8_update(0, end_crc, 2);
        if (!uart_rx_byte_timeout(&crc_rx, 500)) return -1;
        send_ack((end == 0 && crc_rx == crc_val) ? 1 : 0);
        return (end == 0 && crc_rx == crc_val) ? 1 : -1;
    }
}
