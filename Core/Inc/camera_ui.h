/*
 * camera_ui.h — 摄像头预览屏
 */
#ifndef CAMERA_UI_H
#define CAMERA_UI_H

#include <stdint.h>

void camera_ui_create(void);
void camera_ui_show(void);
void camera_ui_hide(void);

extern volatile uint8_t g_camera_active;

#endif
