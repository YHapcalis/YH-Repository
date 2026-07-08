/**
 * @file    touch.c
 * @brief   触摸屏初始化 + 独立轮询 + LVGL indev 桥接
 *          v2: 触摸轮询从 LVGL indev timer 移到 GUI 任务循环,
 *              彻底排除 LVGL timer 调度延迟的影响。
 *              LVGL indev read_cb 只回传缓存的最后状态, 不做 I2C 操作。
 */

#include "touch.h"
#include "gt5663.h"
#include "ctiic.h"
#include "delay.h"
#include <stdio.h>

/* ---- 触摸状态缓存 (touch_poll 更新, touch_read_cb 回传, 跨任务 volatile) ---- */
static volatile uint8_t  g_pressed  = 0;
static volatile uint16_t g_last_x   = 0;
static volatile uint16_t g_last_y   = 0;

/* ---- 初始化 (I2C 扫描 + GT911 复位 + LVGL indev 注册) ---- */
void touch_init(void)
{
    /* DWT 周期计数器 (供 ctiic.c delay_us 使用) */
    delay_init();

    /* 解锁备份域 — PC13 是 RTC 域引脚 */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* GPIO 诊断: 只读当前状态 */
    printf("[TOUCH] GPIO diag: PB0 IDR=0x%08lX MODER=0x%08lX\r\n",
           GPIOB->IDR, GPIOB->MODER);
    printf("[TOUCH] GPIO diag: PF11 IDR=0x%08lX MODER=0x%08lX\r\n",
           GPIOF->IDR, GPIOF->MODER);
    printf("[TOUCH] GPIO diag: PC13 IDR=0x%08lX MODER=0x%08lX\r\n",
           GPIOC->IDR, GPIOC->MODER);

    CT_IIC_Init();

    /* I2C 地址扫描 */
    printf("[TOUCH] I2C scan: ");
    int found = 0;
    for (int addr = 0x08; addr < 0x78; addr++) {
        CT_IIC_Start();
        CT_IIC_Send_Byte((uint8_t)(addr << 1));
        if (CT_IIC_Wait_Ack() == 0) {
            printf("0x%02X ", addr);
            found++;
        }
        CT_IIC_Stop();
    }
    if (found == 0) printf("NONE");
    printf("\r\n");
    if (found == 0) {
        printf("[TOUCH] No I2C device found — check power/soldering\r\n");
    }

    printf("[TOUCH] Init GT5663/GT5688...\r\n");
    if (GT5663_Init() == 0) {
        /* LVGL v8.3 使用 lv_indev_drv_t 注册输入设备 */
        static lv_indev_drv_t indev_drv;
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = touch_read_cb;
        lv_indev_drv_register(&indev_drv);
        printf("[TOUCH] LVGL indev registered\r\n");
    }
}

/* ---- 触摸轮询 — 由 GUI 任务循环直接调用 (预期 ~5ms 一次) ---- */
#define DEBOUNCE_CNT  3    /* 连续 N 次相同状态才确认 (N × 5ms ≈ 15ms) */
void touch_poll(void)
{
    static uint16_t idle_cnt    = 0;
    static uint8_t  db_press    = 0;   /* 连续按下计数 */
    static uint8_t  db_release  = 0;   /* 连续释放计数 */
    static uint16_t db_x        = 0;   /* 消抖期间的坐标缓存 */
    static uint16_t db_y        = 0;

    if (GT5663_Scan()) {
        db_press++;
        db_release = 0;
        /* 即时缓存坐标 — 消抖通过后回传 */
        db_x = g_touch.x[0];
        db_y = g_touch.y[0];

        if (db_press >= DEBOUNCE_CNT) {
            g_pressed = 1;
            g_last_x  = db_x;
            g_last_y  = db_y;
            db_press  = DEBOUNCE_CNT;  /* 钳位 */
        }

        GPIOF->BSRR = (uint32_t)GPIO_PIN_10 << 16U;  /* LED1 ON */
        idle_cnt = 0;
    } else {
        db_release++;
        db_press = 0;

        if (db_release >= DEBOUNCE_CNT) {
            g_pressed = 0;
            db_release = DEBOUNCE_CNT;
        }

        /* 无触摸: LED1 慢闪 (25 × 调用间隔) */
        idle_cnt++;
        if (idle_cnt >= 25) {
            idle_cnt = 0;
            GPIOF->ODR ^= GPIO_PIN_10;
        }
    }
}

/* ---- LVGL indev read_cb — 只返回缓存的最后状态 (极快, 不回阻塞) ---- */
void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    if (g_pressed) {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = g_last_x;
        data->point.y = g_last_y;
    } else {
        data->state   = LV_INDEV_STATE_RELEASED;
    }
}
