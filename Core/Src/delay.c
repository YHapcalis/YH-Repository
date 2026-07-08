/*
 * delay.c — 微秒级延时 (DWT 周期计数器)
 *
 * 配合官方例程使用，由 system.h 声明。
 * 依赖 DWT 外设，内核时钟 168MHz 时需要先使能。
 */

#include "system.h"

static uint8_t g_dwt_ready = 0;

void delay_init(void)
{
    if (g_dwt_ready) return;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    g_dwt_ready = 1;
}

void delay_us(uint32_t us)
{
    if (!g_dwt_ready) delay_init();

    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * 168;  /* 168MHz = 168 周期/微秒 */
    while ((DWT->CYCCNT - start) < ticks);
}

void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
        delay_us(1000);
}
