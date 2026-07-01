/*
 * images.h — LVGL 图片资源声明（来自 uint3code 例程）
 *
 * 6 张图片，已在 Core/Src/images/ 中有 C 数组实现
 */

#ifndef IMAGES_H
#define IMAGES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/* gauge-bg.png — 仪表盘背景 233×233 索引色 */
LV_IMG_DECLARE(ui_img_942215904);

/* gauge-needle.png — 仪表指针 22×86 */
LV_IMG_DECLARE(ui_img_1601502596);

/* Light.png — 灯光指示灯 32×32 */
LV_IMG_DECLARE(ui_img_light_png);

/* safety_belt.png — 安全带指示灯 32×32 */
LV_IMG_DECLARE(ui_img_safety_belt_png);

/* temp_gray.png — 水温指示灯 32×32 */
LV_IMG_DECLARE(ui_img_temp_gray_png);

/* turn_light.png — 转向灯指示灯 32×32 */
LV_IMG_DECLARE(ui_img_turn_light_png);

#ifdef __cplusplus
}
#endif

#endif /* IMAGES_H */
