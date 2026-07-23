/*
 * key.h — 输入事件“统一格式”（KeyEvent 总线）
 *
 * key.c 会把两类输入统一成 KeyEvent：
 * - GPIO 按键（gpio.c 初始化，引脚宏在 main.h）
 * - 旋钮（knob.c 读 TIM1 编码器计数后，通过 Key_PushEvent() 把旋转也塞进队列）
 *
 * app.c 与两个游戏都只需要：
 * - Key_Task() 刷新输入
 * - Key_PopEvent() 取出事件并消费
 */

#ifndef KEY_H
#define KEY_H

#ifdef __cplusplus
extern "C"
{
#endif
#include "main.h"

    typedef enum
    {
        KEY_ID_1 = 0,
        KEY_ID_2,
        KEY_ID_3, // 本工程约定 KEY_ID_3 = 编码器按键 PB15
        // 已禁用KEY_ID_KNOB_LEFT,  // [新增] 虚拟按键：旋钮左旋
        // 已禁用KEY_ID_KNOB_RIGHT, // [新增] 虚拟按键：旋钮右旋
        KEY_ID_COUNT
    } KeyId;

    typedef enum
    {
        KEY_EVENT_NONE = 0,
        KEY_EVENT_SINGLE, // 短按：按下并松开，且持续时间 < KEY_LONG_PRESS_MS
        KEY_EVENT_DOUBLE, // 双击：两次短按之间的间隔 < KEY_DOUBLE_GAP_MS
        KEY_EVENT_LONG,   // 长按：按下持续时间 >= KEY_LONG_PRESS_MS 后松开
        // 已禁用KEY_EVENT_KNOB_TURN, //[新增] 旋钮专用事件类型
        KEY_EVENT_DOWN, // [新增] 瞬间按下事件（用于游戏等极小延迟场景）
    } KeyEventType;

    typedef struct
    {
        KeyId id;
        KeyEventType type;
        uint32_t tick;
    } KeyEvent;

    void Key_Init(void);
    void Key_Task(void);
    /* [新增] 暴露给 knob 等外部模块推入自定义事件用的接口 */
    // 已禁用knob旋钮 void Key_PushEvent(KeyId id, KeyEventType type, uint32_t tick);

    uint8_t Key_PopEvent(KeyEvent *evt);

#ifdef __cplusplus
}
#endif

#endif /* KEY_H */