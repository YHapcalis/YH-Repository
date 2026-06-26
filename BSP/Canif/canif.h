/*
 * canif.h — CAN 接口层（F407 适配）
 *
 * 架构角色：通信能力层。封装 HAL CAN1 的初始化/发送/接收。
 * 调用方（freertos.c canRxTask）轮询接收，解析为 sensor_data 供 UI 使用。
 *
 * 数据流：
 *   F103 (ID=0x12) → CAN 总线 → F407 CAN1_RX
 *     → canRxTask 轮询 → CAN1_Recv_Msg → can_sensor_data
 *     → custom.c timer 读取 → 更新 mode_label_temp_num
 */

#ifndef __CANIF_H__
#define __CANIF_H__

#include "main.h"
#include "can.h"
#include <stdint.h>

/* ── CAN ID 定义 ── */
#define CAN_ID_SENSOR   0x12   /* F103 → F407: 传感器数据帧       */

/* ── F103 传感器数据结构 ── */
/* F103 发送格式（8 字节，带 0.1 精度）:
 *   Byte 0: 温度整数部分 + 40（-40~85℃ → 0~125）
 *   Byte 1: 温度小数部分 ×10（0~9）
 *   Byte 2: 湿度整数部分（0~100）
 *   Byte 3: 湿度小数部分 ×10（0~9）
 *   Byte 4: 旋钮值 0-255
 *   Byte 5: 按键事件类型
 *   Byte 6-7: 保留
 */
typedef struct {
    float   temperature;   /* 摄氏温度                             */
    float   humidity;      /* 相对湿度 (%)                        */
    uint8_t knob;          /* 旋钮值 0-255                        */
    uint8_t key_event;     /* 按键事件类型                         */
    uint8_t valid;         /* 1=数据已收到, 0=尚无数据             */
    uint32_t tick;         /* 收到数据时的 HAL_GetTick() 时间戳    */
} can_sensor_data_t;

/* ── 全局传感器数据（canRxTask 写入, GUI task 读取）── */
extern volatile can_sensor_data_t g_can_sensor;

/* ── 接口函数 ── */
uint8_t canif_init(void);
uint8_t CAN1_Send_Msg(uint32_t id, uint8_t *msg, uint8_t len);
uint8_t CAN1_Recv_Msg(uint32_t *id, uint8_t *buf);

/* 解析 F103 传感器数据帧（buf[0..3] → g_can_sensor） */
void canif_parse_sensor(uint8_t *buf, uint8_t len);

#endif /* __CANIF_H__ */
