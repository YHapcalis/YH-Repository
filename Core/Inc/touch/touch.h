/**
 * @file    touch.h
 * @brief   触摸屏 LVGL 输入设备接口
 */

#ifndef __TOUCH_H
#define __TOUCH_H

#include "lvgl.h"

void touch_init(void);
void touch_poll(void);                          /* GUI 任务循环中调用, 轮询触摸状态 */
void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);

#endif
