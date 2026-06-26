/**
 * @file    lv_port_disp.c
 * @brief   LVGL v9.3 显示端口 — PARTIAL 模式, 缓冲在内部 SRAM
 */

#include "lv_port_disp.h"
#include "nt35510.h"

#define DISP_HOR_RES  800
#define DISP_VER_RES  480
#define BUF_LINES     32
#define BUF_PIXELS    ((uint32_t)DISP_HOR_RES * BUF_LINES)

static uint8_t draw_buf[BUF_PIXELS * sizeof(uint16_t)];

static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map)
{
    uint16_t w = lv_area_get_width(area);
    uint16_t h = lv_area_get_height(area);
    uint16_t *buf16 = (uint16_t *)px_map;
    uint32_t total = (uint32_t)w * h;

    NT35510_SetWindow(area->x1, area->y1, area->x2, area->y2);

    for (uint32_t i = 0; i < total; i++) {
        NT35510->DAT = buf16[i];
    }

    lv_display_flush_ready(disp);
}

void lv_port_disp_init(void)
{
    lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);

    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_flush_cb(disp, disp_flush_cb);
}
