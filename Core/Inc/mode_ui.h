/*
 * mode_ui.h — Mode 屏（信息页 + 诊断页）
 *
 * 通过底部按钮从 Home 屏切换进入，lv_scr_load 轻量切换
 * 不依赖滑动手势，零额外 CPU 开销
 */

#ifndef MODE_UI_H
#define MODE_UI_H

#include "stdint.h"

/* ── Mode 屏创建（在 app_ui_create 后调用一次）── */
void mode_ui_create(void);

/* ── 切换到 Mode 屏 / 返回 Home 屏 ── */
void mode_ui_show(void);
void mode_ui_hide(void);

/* ── 信息页更新（由 sysmon_timer_cb 定期调用）── */
void mode_ui_update_info(uint32_t heap_free, uint32_t heap_total,
                         uint32_t uptime_sec, uint16_t ota_count,
                         uint8_t can_online);

/* ── 主题切换 ── */
void mode_ui_apply_theme(uint8_t theme_id);

#endif /* MODE_UI_H */
