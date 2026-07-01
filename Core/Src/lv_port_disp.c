/*
 * lv_port_disp.c — LVGL v8.3 显示端口 (NT35510 FSMC 8080 16-bit)
 *
 * PARTIAL 模式, 缓冲在内部 SRAM (16 行 × 800px × 2B = 25.6KB)
 * 使用 lv_disp_drv_t (v8.3 标准 API) 而非 v9 的 lv_display_create
 */

#include "lv_port_disp.h"
#include "nt35510.h"
#include "lvgl.h"

#define DISP_HOR_RES  800
#define DISP_VER_RES  480
#define BUF_LINES     16
#define BUF_PIXELS    ((uint32_t)DISP_HOR_RES * BUF_LINES)

/* 绘制缓冲: 16 行像素 */
static uint8_t draw_buf[BUF_PIXELS * sizeof(uint16_t)];

/* ═══════════════════════ LVGL 冲刷回调 ═══════════════════════ */
static void disp_flush_cb(lv_disp_drv_t *disp_drv,
                          const lv_area_t *area, lv_color_t *color_p)
{
    uint16_t w = lv_area_get_width(area);
    uint16_t h = lv_area_get_height(area);
    uint16_t *buf16 = (uint16_t *)color_p;
    uint32_t total = (uint32_t)w * h;

    NT35510_SetWindow(area->x1, area->y1, area->x2, area->y2);

    for (uint32_t i = 0; i < total; i++) {
        /* LV_COLOR_16_SWAP=1 下 LVGL 存储字节序与 NT35510 期望相反，需重排 */
        uint16_t p = buf16[i];
        NT35510->DAT = (p >> 8) | (p << 8);
    }

    lv_disp_flush_ready(disp_drv);
}

/* ═══════════════════════ 显示端口初始化 ═══════════════════════ */
void lv_port_disp_init(void)
{
    static lv_disp_draw_buf_t draw_buf_dsc;
    static lv_disp_drv_t disp_drv;

    /* 初始化绘制缓冲描述符 */
    lv_disp_draw_buf_init(&draw_buf_dsc, draw_buf, NULL, BUF_PIXELS);

    /* 初始化显示驱动 */
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = DISP_HOR_RES;
    disp_drv.ver_res  = DISP_VER_RES;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &draw_buf_dsc;

    /* 注册到 LVGL */
    lv_disp_drv_register(&disp_drv);
}
