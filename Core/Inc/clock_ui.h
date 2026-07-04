/*
 * clock_ui.h — 日期时钟页（日历 + 时间调整）
 * 通过 CAN 将新时间同步到 F103 RTC
 */

#ifndef CLOCK_UI_H
#define CLOCK_UI_H

#include "stdint.h"

void clock_ui_create(void);
void clock_ui_show(void);
void clock_ui_hide(void);

/* 由 app_ui_update_time 调用刷新当前时间显示 */
void clock_ui_set_time(uint8_t year, uint8_t month, uint8_t day,
                        uint8_t hour, uint8_t min, uint8_t sec);

#endif /* CLOCK_UI_H */
