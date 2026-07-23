/* SG-90 舵机模块头文件 */

#ifndef __SG_90_H__
#define __SG_90_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    void SG90_Init(void);
    void SG90_SetAngle(int8_t rel_angle);
    void SG90_Update(void);

    /* 低层接口，保留给调试场景 */
    void SG90_SetPulse(uint32_t pulse);

#ifdef __cplusplus
}
#endif

#endif /* __SG_90_H__ */