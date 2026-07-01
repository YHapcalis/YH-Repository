
#ifndef DRV8833_H
#define DRV8833_H

#include <stdint.h>
#include "tim.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void DRV8833_Init(TIM_HandleTypeDef *htim, uint32_t ch_fwd, uint32_t ch_rev);

    /* 百分比占空比：0..100（跟教学例程的 85/99 直观对应） */
    void DRV8833_Forward(uint8_t duty_percent);
    void DRV8833_Reverse(uint8_t duty_percent);
    void DRV8833_Stop(void);

#ifdef __cplusplus
}
#endif

#endif