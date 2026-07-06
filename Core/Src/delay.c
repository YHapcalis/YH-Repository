/*
 * delay.c — 微秒级延时 (DWT 周期计数器)
 *
 * 配合官方例程使用，由 system.h 声明。
 * 依赖 DWT 外设，内核时钟 168MHz 时需要先使能。
 */

#include "system.h"

void delay_us(uint32_t us)
{
    static uint8_t initialized = 0;
    if (!initialized) {
        /* 使能 DWT 周期计数器 */
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        initialized = 1;
    }

    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * 168;  /* 168MHz = 168 周期/微秒 */
    while ((DWT->CYCCNT - start) < ticks);
}

void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
        delay_us(1000);
}
