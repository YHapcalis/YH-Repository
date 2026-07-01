/*
 * app_ui.h — APP 精简 GUI (LVGL 8.3 手写)
 *
 * 基于 uint3code 32Car_APP 的图片资源，适配 800×480 NT35510
 */

#ifndef APP_UI_H
#define APP_UI_H

#include "stdint.h"

/* ── UI 初始化 ── */
void app_ui_create(void);

/* ── 传感器数据更新（由 CAN 任务调用）── */
void app_ui_update_sensor(float temp, float hum, uint16_t knob, uint8_t key_id, uint8_t key_type);

/* ── CAN 状态更新 ── */
void app_ui_set_can_status(uint8_t online);

/* ── OTA 状态更新 ── */
void app_ui_set_ota_status(uint8_t updating);

/* ── 指示灯颜色设置 ── */
void app_ui_set_indicator_light(uint8_t color);
void app_ui_set_indicator_watertemp(uint8_t color);
void app_ui_set_indicator_turnlight(uint8_t color);
void app_ui_set_indicator_safetybelt(uint8_t color);

#endif /* APP_UI_H */
