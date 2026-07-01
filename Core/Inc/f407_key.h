/*
 * f407_key.h — F407 原生按键模块
 *
 * 支持 4 个按键：KEY_UP(PA0), KEY1(PE3), KEY2(PE2), KEY0(PE4)
 * 事件类型：SINGLE（短按）、LONG（长按 >800ms）
 *
 * 调用方式：
 *   Key_Init();
 *   // 在主循环或定时器中调用 Key_Task()
 *   KeyEvent evt;
 *   if (Key_PopEvent(&evt)) { ... }
 */

#ifndef __F407_KEY_H__
#define __F407_KEY_H__

#include "main.h"
#include <stdint.h>

#define F407_KEY_COUNT 4

typedef enum {
    F407_KEY_UP = 0,   /* PA0, WK_UP, Active HIGH */
    F407_KEY_1,        /* PE3, Active LOW */
    F407_KEY_2,        /* PE2, Active LOW */
    F407_KEY_0         /* PE4, Active LOW */
} F407_KeyId;

typedef enum {
    F407_KEY_EVENT_NONE   = 0,
    F407_KEY_EVENT_SINGLE,
    F407_KEY_EVENT_LONG
} F407_KeyEventType;

typedef struct {
    F407_KeyId        id;
    F407_KeyEventType type;
} F407_KeyEvent;

void F407_Key_Init(void);
void F407_Key_Task(void);
uint8_t F407_Key_PopEvent(F407_KeyEvent *evt);

#endif /* __F407_KEY_H__ */
