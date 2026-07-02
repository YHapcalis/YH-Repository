/*
 * ymodem_porting.c — Ymodem UART 移植层（F407 USART1 ↔ CH340C）
 *
 * 从 uint3code 例程 34Bootloader/ymodem/ymodem_porting.c 适配
 * 修改: huart2 → huart1（我们的 CH340C 在 USART1）
 */

#include "ymodem_porting.h"
#include "usart.h"

/* huart1 定义在 usart.c */
extern UART_HandleTypeDef huart1;

void Serial_PutString(uint8_t *p_string)
{
    uint16_t length = 0;
    while (p_string[length] != '\0') {
        length++;
    }
    HAL_UART_Transmit(&huart1, p_string, length, TX_TIMEOUT);
}

HAL_StatusTypeDef Serial_PutByte(uint8_t param)
{
    return HAL_UART_Transmit(&huart1, &param, 1, TX_TIMEOUT);
}

HAL_StatusTypeDef Serial_Recv_data(uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    (void)Timeout;
    uint32_t cnt_max = 80000000;  /* ~7.5s 单字节超时 @ 168MHz (实测) */
    for (uint16_t i = 0; i < Size; i++) {
        uint32_t cnt = 0;
        while (!(USART1->SR & USART_SR_RXNE)) {
            if (++cnt >= cnt_max) return HAL_TIMEOUT;
        }
        pData[i] = USART1->DR;
    }
    return HAL_OK;
}
