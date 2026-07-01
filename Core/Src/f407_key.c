/*
 * f407_key.c — F407 原生按键实现
 *
 * 4 个按键，支持短按 (SINGLE) 和长按 (LONG)。
 * KEY_UP(PA0) 为 Active HIGH，其余为 Active LOW。
 */

#include "f407_key.h"

#define DEBOUNCE_MS   20U
#define LONG_PRESS_MS 800U
#define EVENT_QSIZE   4U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       active_high;   /* 1=HIGH 按下, 0=LOW 按下 */
    uint8_t       is_down;
    uint32_t      press_tick;
    uint32_t      last_edge_tick;
} KeyState;

static KeyState s_keys[F407_KEY_COUNT];
static F407_KeyEvent s_q[EVENT_QSIZE];
static uint8_t s_q_head = 0;
static uint8_t s_q_tail = 0;

static void push_event(F407_KeyId id, F407_KeyEventType type)
{
    uint8_t next = (s_q_head + 1) % EVENT_QSIZE;
    if (next == s_q_tail) return;
    s_q[s_q_head].id   = id;
    s_q[s_q_head].type = type;
    s_q_head = next;
}

void F407_Key_Init(void)
{
    s_q_head = 0;
    s_q_tail = 0;

    /* KEY_UP: PA0, WK_UP — Active HIGH */
    s_keys[F407_KEY_UP].port        = KEY_UP_GPIO_Port;
    s_keys[F407_KEY_UP].pin         = KEY_UP_Pin;
    s_keys[F407_KEY_UP].active_high = 1;
    s_keys[F407_KEY_UP].is_down     = 0;

    /* KEY1: PE3 — Active LOW */
    s_keys[F407_KEY_1].port        = KEY1_GPIO_Port;
    s_keys[F407_KEY_1].pin         = KEY1_Pin;
    s_keys[F407_KEY_1].active_high = 0;
    s_keys[F407_KEY_1].is_down     = 0;

    /* KEY2: PE2 — Active LOW */
    s_keys[F407_KEY_2].port        = KEY2_GPIO_Port;
    s_keys[F407_KEY_2].pin         = KEY2_Pin;
    s_keys[F407_KEY_2].active_high = 0;
    s_keys[F407_KEY_2].is_down     = 0;

    /* KEY0: PE4 — Active LOW */
    s_keys[F407_KEY_0].port        = KEY0_GPIO_Port;
    s_keys[F407_KEY_0].pin         = KEY0_Pin;
    s_keys[F407_KEY_0].active_high = 0;
    s_keys[F407_KEY_0].is_down     = 0;

    uint32_t now = HAL_GetTick();
    for (int i = 0; i < F407_KEY_COUNT; i++) {
        s_keys[i].press_tick      = 0;
        s_keys[i].last_edge_tick  = now;
    }
}

void F407_Key_Task(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < F407_KEY_COUNT; i++) {
        KeyState *k = &s_keys[i];
        GPIO_PinState level = HAL_GPIO_ReadPin(k->port, k->pin);

        /* Active HIGH → 按下=SET, Active LOW → 按下=RESET */
        uint8_t pressed = (k->active_high) ?
                          (level == GPIO_PIN_SET) :
                          (level == GPIO_PIN_RESET);

        /* 消抖 */
        if (pressed != k->is_down) {
            if (now - k->last_edge_tick >= DEBOUNCE_MS) {
                k->last_edge_tick = now;
                k->is_down = pressed;
                if (pressed) {
                    k->press_tick = now;
                } else {
                    /* 释放: 判断短按还是长按 */
                    if (now - k->press_tick >= LONG_PRESS_MS) {
                        push_event((F407_KeyId)i, F407_KEY_EVENT_LONG);
                    } else {
                        push_event((F407_KeyId)i, F407_KEY_EVENT_SINGLE);
                    }
                }
            }
        }
    }
}

uint8_t F407_Key_PopEvent(F407_KeyEvent *evt)
{
    if (evt == NULL) return 0;
    if (s_q_tail == s_q_head) return 0;
    *evt = s_q[s_q_tail];
    s_q_tail = (s_q_tail + 1) % EVENT_QSIZE;
    return 1;
}
