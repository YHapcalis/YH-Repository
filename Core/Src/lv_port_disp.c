/**
 * @file    lv_port_disp.c
 * @brief   LVGL v9.5 显示端口 — NT35510 LCD (FSMC 8080 16-bit, 800×480)
 *
 * 绘制缓冲: 外部 SRAM (IS62WV51216 @ 0x68000000, 1MB)
 *   双缓冲, 各 48 行 × 800 像素 × 2 字节 = 76.8KB, 总计 ~154KB
 *   SRAM 剩余 ~870KB 可供 LVGL 图片缓存 / 字体缓存
 *
 * 刷新策略: LVGL 渲染到 draw_buf → flush_cb 调用 NT35510 写入 LCD
 */

#include "lv_port_disp.h"
#include "nt35510.h"
#include <string.h>

/* ================================================================
 * 绘制缓冲 — 驻留外部 SRAM
 * ================================================================ */

#define EXT_SRAM_BASE       ((uint32_t)0x68000000)

#define DISP_HOR_RES        800
#define DISP_VER_RES        480
#define BUF_LINES           48      /* 每缓冲 48 行 (= 1/10 屏) */
#define BUF_PIXELS          ((uint32_t)DISP_HOR_RES * BUF_LINES)
#define BUF_BYTES           (BUF_PIXELS * sizeof(uint16_t))

/* 双缓冲映射到外部 SRAM 地址空间 */
static uint16_t *draw_buf_1 = (uint16_t *)(EXT_SRAM_BASE);
static uint16_t *draw_buf_2 = (uint16_t *)(EXT_SRAM_BASE + BUF_BYTES);

/* ================================================================
 * flush 回调 — LVGL 渲染完成后调用, 将像素写入 LCD
 * ================================================================ */
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map)
{
    uint16_t w = lv_area_get_width(area);
    uint16_t h = lv_area_get_height(area);
    uint16_t *buf16 = (uint16_t *)px_map;
    uint32_t total = (uint32_t)w * h;

    /* 设置 LCD 写入窗口 → 连续写像素 */
    NT35510_SetWindow(area->x1, area->y1, area->x2, area->y2);

    for (uint32_t i = 0; i < total; i++) {
        NT35510->DAT = buf16[i];
    }

    /* 通知 LVGL 刷新完成 */
    lv_display_flush_ready(disp);
}

/* ================================================================
 * 初始化 LVGL 显示
 * ================================================================ */
void lv_port_disp_init(void)
{
    /* 重置外部 SRAM 缓冲 (首次使用前清零, 避免随机像素) */
    memset(draw_buf_1, 0, BUF_BYTES);
    memset(draw_buf_2, 0, BUF_BYTES);

    /* 创建 LVGL display 对象 */
    lv_display_t *disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);

    /* 设置绘制缓冲 — 双缓冲模式 */
    lv_display_set_buffers(disp, draw_buf_1, draw_buf_2,
                           BUF_PIXELS * sizeof(uint16_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 注册 flush 回调 */
    lv_display_set_flush_cb(disp, disp_flush_cb);
}
