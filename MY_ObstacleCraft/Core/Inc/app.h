#ifndef APP_H
#define APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        STATE_IDLE = 0,
        STATE_FORWARD,
        STATE_CAUTION,
        STATE_AVOID,
        STATE_RECOVERY,
        STATE_FAULT
    } AppState;

    extern int16_t g_distance_cm;
    extern uint8_t g_DRV8833_pwm;
    extern int8_t g_DRV8833_dir;
    extern int8_t g_SG90_angle;
    extern AppState g_app_state;

    void app_init(void);
    void app_run(void);

    void App_Init(void);
    void App_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */