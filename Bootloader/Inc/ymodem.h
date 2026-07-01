/*
 * ymodem.h — Ymodem 协议声明（F407 适配版）
 */

#ifndef __YMODEM_H__
#define __YMODEM_H__

#include <stdint.h>

typedef enum {
    COM_OK       = 0x00,
    COM_ERROR    = 0x01,
    COM_ABORT    = 0x02,
    COM_TIMEOUT  = 0x03,
    COM_DATA     = 0x04,
    COM_LIMIT    = 0x05
} COM_StatusTypeDef;

#define FILE_NAME_LENGTH        ((uint32_t)64)

COM_StatusTypeDef Ymodem_Receive(uint32_t *p_size);

/* 文件名缓冲（Ymodem 首包提取，供调用方读取） */
extern uint8_t aFileName[FILE_NAME_LENGTH];

#endif /* __YMODEM_H__ */
