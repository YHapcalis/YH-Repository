/*
 * ymodem_porting.h — Ymodem 串口移植层（F407 USART1 ↔ CH340C）
 */

#ifndef __YMODEM_PORTING_H__
#define __YMODEM_PORTING_H__

#include "stm32f4xx_hal.h"

#define TX_TIMEOUT          ((uint32_t)100)
#define RX_TIMEOUT          HAL_MAX_DELAY

void Serial_PutString(uint8_t *p_string);
HAL_StatusTypeDef Serial_PutByte(uint8_t param);
HAL_StatusTypeDef Serial_Recv_data(uint8_t *pData, uint16_t Size, uint32_t Timeout);

#endif /* __YMODEM_PORTING_H__ */
