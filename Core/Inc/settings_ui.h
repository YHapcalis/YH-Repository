/*
 * settings_ui.h — 设置页（系统操作 + 主题选择）
 */

#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include "stdint.h"

void settings_ui_create(void);
void settings_ui_show(void);
void settings_ui_hide(void);

/* ── 主题切换（由 app_ui.c 统一调用）── */
void settings_ui_apply_theme(uint8_t theme_id);

#endif /* SETTINGS_UI_H */
