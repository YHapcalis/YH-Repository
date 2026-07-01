/*
 * ymodem.c — Ymodem 协议接收器（F407 适配版）
 *
 * 基于 ST 官方 IAP 示例 + uint3code 例程移植
 * 适配 F407 Flash: 预擦除 APP 全扇区 → 逐 32-bit 字写入
 *
 * 协议: 128/1024 字节包, CRC16 校验
 */

#include "ymodem.h"
#include "ymodem_porting.h"
#include "inter_flashif.h"
#include <string.h>
#include <stdio.h>

/* ── Ymodem 协议常量 ── */
#define PACKET_HEADER_SIZE      ((uint32_t)3)
#define PACKET_DATA_INDEX       ((uint32_t)4)
#define PACKET_START_INDEX      ((uint32_t)1)
#define PACKET_NUMBER_INDEX     ((uint32_t)2)
#define PACKET_CNUMBER_INDEX    ((uint32_t)3)
#define PACKET_TRAILER_SIZE     ((uint32_t)2)
#define PACKET_OVERHEAD_SIZE    (PACKET_HEADER_SIZE + PACKET_TRAILER_SIZE - 1)
#define PACKET_SIZE             ((uint32_t)128)
#define PACKET_1K_SIZE          ((uint32_t)1024)

#define FILE_NAME_LENGTH        ((uint32_t)64)
#define FILE_SIZE_LENGTH        ((uint32_t)16)

#define SOH                     ((uint8_t)0x01)
#define STX                     ((uint8_t)0x02)
#define EOT                     ((uint8_t)0x04)
#define ACK                     ((uint8_t)0x06)
#define NAK                     ((uint8_t)0x15)
#define CA                      ((uint32_t)0x18)
#define CRC16                   ((uint8_t)0x43)
#define NEGATIVE_BYTE           ((uint8_t)0xFF)

#define ABORT1                  ((uint8_t)0x41)
#define ABORT2                  ((uint8_t)0x61)

#define NAK_TIMEOUT             ((uint32_t)0x100000)
#define DOWNLOAD_TIMEOUT        ((uint32_t)1000)
#define MAX_ERRORS              ((uint32_t)5)

/* ── Flash 写入策略 ── */
#define FLASH_BUF_SIZE          2048    /* 累计 2048 字节后写一次 Flash */

/* ── 全局缓冲 ── */
uint8_t aPacketData[PACKET_1K_SIZE + PACKET_DATA_INDEX + PACKET_TRAILER_SIZE];
uint8_t aFileName[FILE_NAME_LENGTH];
static uint8_t flash_buf[FLASH_BUF_SIZE] = {0};
static uint32_t flash_buf_rx_cnt = 0;
static uint32_t page_offset = 0;        /* 当前写入偏移（相对 APP 起始地址） */

/* ── 内部函数声明 ── */
static HAL_StatusTypeDef ReceivePacket(uint8_t *p_data, uint32_t *p_length, uint32_t timeout);
static uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte);
static uint16_t Cal_CRC16(const uint8_t *p_data, uint32_t size);
static void     write_flush(void);
static uint8_t  erase_app_area(uint32_t file_size);
static void     Int2Str(uint8_t *p_str, uint32_t intval);
static uint32_t Str2Int(uint8_t *p_input_str, uint32_t *p_intval);

/* ═══════════════════════════════════════════════════════════
 *  ReceivePacket — 接收一个 Ymodem 数据包
 * ═══════════════════════════════════════════════════════════ */
static HAL_StatusTypeDef ReceivePacket(uint8_t *p_data, uint32_t *p_length, uint32_t timeout)
{
    uint32_t crc;
    uint32_t packet_size = 0;
    HAL_StatusTypeDef status;
    uint8_t char1;

    *p_length = 0;
    status = Serial_Recv_data(&char1, 1, timeout);

    if (status == HAL_OK) {
        switch (char1) {
        case SOH:
            packet_size = PACKET_SIZE;
            break;
        case STX:
            packet_size = PACKET_1K_SIZE;
            break;
        case EOT:
            break;
        case CA:
            if ((Serial_Recv_data(&char1, 1, timeout) == HAL_OK) && (char1 == CA))
                packet_size = 2;
            else
                status = HAL_ERROR;
            break;
        case ABORT1:
        case ABORT2:
            status = HAL_BUSY;
            break;
        default:
            status = HAL_ERROR;
            break;
        }
        *p_data = char1;

        if (packet_size >= PACKET_SIZE) {
            status = Serial_Recv_data(&p_data[PACKET_NUMBER_INDEX],
                                       packet_size + PACKET_OVERHEAD_SIZE, timeout);
            if (status == HAL_OK) {
                if (p_data[PACKET_NUMBER_INDEX] !=
                    ((p_data[PACKET_CNUMBER_INDEX]) ^ NEGATIVE_BYTE)) {
                    packet_size = 0;
                    status = HAL_ERROR;
                } else {
                    crc = p_data[packet_size + PACKET_DATA_INDEX] << 8;
                    crc += p_data[packet_size + PACKET_DATA_INDEX + 1];
                    if (Cal_CRC16(&p_data[PACKET_DATA_INDEX], packet_size) != crc) {
                        packet_size = 0;
                        status = HAL_ERROR;
                    }
                }
            } else {
                packet_size = 0;
            }
        }
    }
    *p_length = packet_size;
    return status;
}

/* ═══════════════════════════════════════════════════════════
 *  CRC16 计算
 * ═══════════════════════════════════════════════════════════ */
static uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte)
{
    uint32_t crc = crc_in;
    uint32_t in = byte | 0x100;
    do {
        crc <<= 1;
        in <<= 1;
        if (in & 0x100) ++crc;
        if (crc & 0x10000) crc ^= 0x1021;
    } while (!(in & 0x10000));
    return crc & 0xffffu;
}

static uint16_t Cal_CRC16(const uint8_t *p_data, uint32_t size)
{
    uint32_t crc = 0;
    const uint8_t *dataEnd = p_data + size;
    while (p_data < dataEnd)
        crc = UpdateCRC16(crc, *p_data++);
    crc = UpdateCRC16(crc, 0);
    crc = UpdateCRC16(crc, 0);
    return crc & 0xffffu;
}

/* ═══════════════════════════════════════════════════════════
 *  写入累积缓冲到 Flash（F407: 按 32-bit 字写入）
 * ═══════════════════════════════════════════════════════════ */
static void write_flush(void)
{
    if (flash_buf_rx_cnt == 0) return;

    uint32_t write_addr = INTER_FLASH_APP_ADDR + page_offset;
    uint32_t word_len = flash_buf_rx_cnt / 4;
    uint32_t remainder = flash_buf_rx_cnt % 4;

    /* 写入完整的 32-bit 字 */
    if (word_len > 0) {
        if (inter_flashif_write(write_addr, (uint32_t *)flash_buf, word_len) != 0) {
            printf("[YMODEM] Write FAIL at 0x%08lX\r\n", write_addr);
        }
    }

    /* 处理末尾不足 4 字节的数据（用 0xFF 填充） */
    if (remainder > 0) {
        uint32_t last_word = 0xFFFFFFFF;
        memcpy(&last_word, flash_buf + (word_len * 4), remainder);
        if (inter_flashif_write(write_addr + (word_len * 4), &last_word, 1) != 0) {
            printf("[YMODEM] Tail write FAIL at 0x%08lX\r\n",
                   write_addr + (word_len * 4));
        }
    }

    printf("[YMODEM] Write 0x%08lX (%lu bytes)\r\n",
           write_addr, flash_buf_rx_cnt);
    page_offset += flash_buf_rx_cnt;
    flash_buf_rx_cnt = 0;
    memset(flash_buf, 0, FLASH_BUF_SIZE);
}

/* ═══════════════════════════════════════════════════════════
 *  预擦除 APP 占用区域（按 file_size 计算所需 sector）
 * ═══════════════════════════════════════════════════════════ */
static uint8_t erase_app_area(uint32_t file_size)
{
    uint32_t end_addr = INTER_FLASH_APP_ADDR + file_size;
    uint32_t addr;
    uint32_t count = 0;

    /* 从 APP 起始地址开始，逐 sector 向上擦除直到覆盖所有文件 */
    addr = INTER_FLASH_APP_ADDR;
    while (addr < end_addr) {
        printf("[YMODEM] Erasing sector @ 0x%08lX...\r\n", addr);
        if (inter_flashif_erase_sector(addr) != 0) {
            printf("[YMODEM] Erase FAIL at 0x%08lX\r\n", addr);
            return 1;
        }
        count++;

        /* 跳到下一个 sector 的起始地址 */
        if (addr < 0x08010000)          addr += 0x4000;  /* 16KB sectors (2,3) */
        else if (addr < 0x08020000)     addr += 0x10000; /* 64KB sector (4) */
        else                            addr += 0x20000; /* 128KB sectors (5-10) */
    }

    printf("[YMODEM] Erased %lu sectors\r\n", count);
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Ymodem_Receive — 接收固件并写入 Flash
 * ═══════════════════════════════════════════════════════════ */
COM_StatusTypeDef Ymodem_Receive(uint32_t *p_size)
{
    uint32_t i, packet_length, session_done = 0, file_done, errors = 0;
    uint32_t flashdestination, filesize;
    uint8_t *file_ptr;
    uint8_t file_size[FILE_SIZE_LENGTH] = {0};
    COM_StatusTypeDef result = COM_OK;
    uint32_t packets_received;
    uint32_t file_all_num = 0;
    uint8_t erase_done = 0;

    flashdestination = INTER_FLASH_APP_ADDR;
    printf("[YMODEM] Enter Ymodem_Receive\r\n");

    while ((session_done == 0) && (result == COM_OK)) {
        packets_received = 0;
        file_done = 0;
        file_all_num = 0;
        flash_buf_rx_cnt = 0;
        page_offset = 0;

        while ((file_done == 0) && (result == COM_OK)) {
            switch (ReceivePacket(aPacketData, &packet_length, DOWNLOAD_TIMEOUT)) {
            case HAL_OK:
                errors = 0;
                switch (packet_length) {
                case 2: /* 取消 */
                    Serial_PutByte(ACK);
                    result = COM_ABORT;
                    break;

                case 0: /* 传输结束 */
                    printf("[YMODEM] Transfer complete\r\n");
                    /* 刷写剩余缓冲 */
                    write_flush();
                    Serial_PutByte(ACK);
                    Serial_PutByte(CRC16);
                    file_done = 1;
                    session_done = 1;
                    break;

                default: /* 普通数据包 */
                    if ((packets_received % 256) != (uint32_t)aPacketData[PACKET_NUMBER_INDEX]) {
                        printf("[YMODEM] SEQ err\r\n");
                        Serial_PutByte(NAK);
                    } else {
                        if (packets_received == 0) {
                            /* ── 首包：文件名 + 大小 ── */
                            if (aPacketData[PACKET_DATA_INDEX] != 0) {
                                /* 提取文件名 */
                                i = 0;
                                file_ptr = aPacketData + PACKET_DATA_INDEX;
                                while ((*file_ptr != 0) && (i < FILE_NAME_LENGTH))
                                    aFileName[i++] = *file_ptr++;
                                aFileName[i] = '\0';

                                /* 提取文件大小 */
                                i = 0;
                                file_ptr++;
                                while ((*file_ptr != ' ') && (i < FILE_SIZE_LENGTH))
                                    file_size[i++] = *file_ptr++;
                                file_size[i] = '\0';
                                Str2Int(file_size, &filesize);
                                *p_size = filesize;
                                file_all_num = (filesize + PACKET_SIZE - 1) / PACKET_SIZE;

                                printf("[YMODEM] File: %s  Size: %lu bytes\r\n",
                                       aFileName, filesize);

                                /* 预擦除 APP 扇区 */
                                if (!erase_done) {
                                    if (erase_app_area(filesize) != 0) {
                                        result = COM_ERROR;
                                        break;
                                    }
                                    erase_done = 1;
                                }

                                Serial_PutByte(ACK);
                                Serial_PutByte(CRC16);
                            } else {
                                /* 空首包 = 结束会话 */
                                Serial_PutByte(ACK);
                                file_done = 1;
                                session_done = 1;
                                break;
                            }
                        } else {
                            /* ── 数据包：累积到缓冲 ── */
                            memcpy(&flash_buf[flash_buf_rx_cnt],
                                   &aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
                            flash_buf_rx_cnt += PACKET_SIZE;

                            /* 每 2048 字节刷写一次 Flash */
                            if (flash_buf_rx_cnt >= FLASH_BUF_SIZE) {
                                write_flush();
                            }

                            Serial_PutByte(ACK);
                        }
                        packets_received++;
                    }
                    break;
                }
                break;

            case HAL_BUSY:
                Serial_PutByte(CA);
                Serial_PutByte(CA);
                result = COM_ABORT;
                break;

            default:
                if (errors > MAX_ERRORS) {
                    Serial_PutByte(CA);
                    Serial_PutByte(CA);
                    result = COM_ERROR;
                } else {
                    errors++;
                    Serial_PutByte(CRC16);
                }
                break;
            }
        }
    }

    printf("[YMODEM] Result: %d\r\n", result);
    return result;
}

/* ═══════════════════════════════════════════════════════════
 *  工具函数
 * ═══════════════════════════════════════════════════════════ */
static void Int2Str(uint8_t *p_str, uint32_t intval)
{
    uint32_t i = 0, j = 0;
    uint8_t tmp[11] = {0};
    if (intval == 0) {
        p_str[0] = '0';
        p_str[1] = '\0';
        return;
    }
    while (intval) {
        tmp[i++] = (uint8_t)('0' + (intval % 10));
        intval /= 10;
    }
    while (i > 0) p_str[j++] = tmp[--i];
    p_str[j] = '\0';
}

static uint32_t Str2Int(uint8_t *p_input_str, uint32_t *p_intval)
{
    uint32_t value = 0, i = 0;
    if (p_input_str == NULL) return 0;
    while (p_input_str[i] >= '0' && p_input_str[i] <= '9') {
        value = (value * 10) + (p_input_str[i] - '0');
        i++;
    }
    *p_intval = value;
    return i;
}
