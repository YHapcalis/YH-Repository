/* HC-SR04 非阻塞测距接口 */

#ifndef HC_SR04_H
#define HC_SR04_H

#include <stdint.h>
#include "main.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void HC_SR04_Init(void);
    void HC_SR04_Trigger(void);
    int16_t HC_SR04_GetDistance(void);

    /* 兼容旧接口：保持工程内历史调用可继续编译 */
    void HCSR04_Init(TIM_HandleTypeDef *htim,
                     GPIO_TypeDef *trig_port, uint16_t trig_pin,
                     GPIO_TypeDef *echo_port, uint16_t echo_pin);
    float HCSR04_GetDistanceCM(void);

#ifdef __cplusplus
}
#endif

#endif /* HC_SR04_H */