/*
 * wifi_config.c — ESP8266 WiFi 数据缓冲 & printf (官方例程适配版)
 *
 * USART3 初始化由 CubeMX 完成，中断处理在 stm32f4xx_it.c。
 * 保留 ESP8266 数据结构 + USART3_printf (适配 HAL)。
 */

#include "wifi_config.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ESP8266 接收数据帧缓冲区 */
struct STRUCT_USARTx_Fram strEsp8266_Fram_Record = { 0 };

/* USART3 格式化输出 (直接寄存器操作) */
void USART3_printf(USART_TypeDef *USARTx, char *Data, ...)
{
    const char *s;
    int d;
    char buf[16];

    va_list ap;
    va_start(ap, Data);

    while (*Data != 0) {
        if (*Data == '\\') {
            switch (*++Data) {
                case 'r': USARTx->DR = 0x0d; Data++; break;
                case 'n': USARTx->DR = 0x0a; Data++; break;
                default: Data++; break;
            }
            while (!(USARTx->SR & USART_SR_TXE));
        } else if (*Data == '%') {
            switch (*++Data) {
                case 's':
                    s = va_arg(ap, const char *);
                    for (; *s; s++) {
                        USARTx->DR = *s;
                        while (!(USARTx->SR & USART_SR_TXE));
                    }
                    Data++;
                    break;
                case 'd':
                    d = va_arg(ap, int);
                    snprintf(buf, sizeof(buf), "%d", d);
                    for (s = buf; *s; s++) {
                        USARTx->DR = *s;
                        while (!(USARTx->SR & USART_SR_TXE));
                    }
                    Data++;
                    break;
                default:
                    Data++;
                    break;
            }
        } else {
            USARTx->DR = *Data++;
            while (!(USARTx->SR & USART_SR_TXE));
        }
    }
}
