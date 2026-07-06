/**
 * @file    spi_img_loader.c
 * @brief   SPI Flash (LittleFS) → 外部 SRAM 图片预加载
 *
 * 6 张图片存放于 SPI Flash 的 LittleFS 文件系统中，启动时由 LFS
 * 读取到外部 SRAM (FSMC @ 0x68000000)，LVGL 直接从 SRAM 渲染。
 *
 * 内部 Flash 后备图片已移除（释放 ~57KB），LFS 文件缺失时无法自动补写。
 * 生产环境需通过 make_lfs_image.py 预先生成 LFS 镜像并烧录。
 *
 * LFS 文件名:
 *   gauge.bin      – gauge-bg     (233×233 INDEXED_4BIT, 27325B)
 *   needle.bin     – needle       (22×86  TRUE_COLOR_ALPHA, 5676B)
 *   light.bin      – light        (32×32  TRUE_COLOR_ALPHA, 3072B)
 *   watertemp.bin  – watertemp    (32×32  TRUE_COLOR_ALPHA, 3072B)
 *   turnlight.bin  – turnlight    (32×32  TRUE_COLOR_ALPHA, 3072B)
 *   safetybelt.bin – safetybelt   (32×32  TRUE_COLOR_ALPHA, 3072B)
 */

#include "spi_img_loader.h"
#include "en25q128.h"
#include "lfs_port.h"
#include <string.h>
#include <stdio.h>

#define SRAM_BASE    ((uint8_t *)0x68000000UL)

/* 图片元数据表 */
typedef struct {
    const char  *filename;   /* LFS 中的文件名     */
    uint32_t     data_size;  /* 像素数据大小        */
    uint16_t     w, h;       /* 宽高               */
    uint8_t      cf;         /* LV_IMG_CF_xxx      */
    lv_img_dsc_t *dsc;      /* SRAM 中的描述符     */
} img_entry_t;

/* ── SRAM 中的描述符（加载后填充 .data 指针）── */
static lv_img_dsc_t s_dsc_gb, s_dsc_nd, s_dsc_lt, s_dsc_wt, s_dsc_tl, s_dsc_sb;

/* 导出 */
lv_img_dsc_t img_sram_gauge_bg;
lv_img_dsc_t img_sram_needle;
lv_img_dsc_t img_sram_light;
lv_img_dsc_t img_sram_watertemp;
lv_img_dsc_t img_sram_turnlight;
lv_img_dsc_t img_sram_safetybelt;

static const img_entry_t s_imgs[] = {
    {"gauge.bin",     27325, 233, 233, LV_IMG_CF_INDEXED_4BIT,    &s_dsc_gb},
    {"needle.bin",     5676,  22,  86, LV_IMG_CF_TRUE_COLOR_ALPHA, &s_dsc_nd},
    {"light.bin",      3072,  32,  32, LV_IMG_CF_TRUE_COLOR_ALPHA, &s_dsc_lt},
    {"watertemp.bin",  3072,  32,  32, LV_IMG_CF_TRUE_COLOR_ALPHA, &s_dsc_wt},
    {"turnlight.bin",  3072,  32,  32, LV_IMG_CF_TRUE_COLOR_ALPHA, &s_dsc_tl},
    {"safetybelt.bin", 3072,  32,  32, LV_IMG_CF_TRUE_COLOR_ALPHA, &s_dsc_sb},
};

/* 数据源: 0=无图片(显示空白), 1=SPI→SRAM, 2=内部Flash(已移除) */
static uint8_t s_src = 0;

uint8_t spi_img_get_source(void) { return s_src; }

/* 导出指针数组（供 app_ui.c 方便索引） */
static lv_img_dsc_t *s_sram_dscs[] = {
    &img_sram_gauge_bg, &img_sram_needle,
    &img_sram_light,    &img_sram_watertemp,
    &img_sram_turnlight, &img_sram_safetybelt,
};

static void spi_load_to_ram(uint8_t *buf);

/* ═════════════════════════════════════════════════════════════
 *  加载全部图片
 * ═════════════════════════════════════════════════════════════ */
void spi_img_load_all(void)
{
    /* 测试外部 SRAM */
    volatile uint32_t *st = (volatile uint32_t *)0x68000000;
    uint32_t orig = st[0];
    st[0] = 0x12345678;
    if (st[0] != 0x12345678) {
        st[0] = orig;
        printf("[IMG] SRAM FAIL, no images available\n");
        s_src = 0;  /* 无图可用 */
        return;
    }
    st[0] = orig;

    /* ---- LFS → 外部 SRAM ---- */
    printf("[IMG] Loading images from LFS to SRAM...\n");
    s_src = 1;
    spi_load_to_ram(SRAM_BASE);
}

static void spi_load_to_ram(uint8_t *buf)
{
    uint32_t total = 0;
    uint32_t loaded_count = 0;

    for (int i = 0; i < 6; i++) {
        const img_entry_t *e = &s_imgs[i];
        uint32_t sz     = e->data_size;
        uint32_t aligned = (sz + 3) & ~3;

        /* 打开 LFS 文件 */
        lfs_file_t f;
        int err = lfs_file_open(&g_lfs, &f, e->filename, LFS_O_RDONLY);
        if (err) {
            printf("[IMG] WARN: %s not found on LFS (%d)\n", e->filename, err);
            /* 无后备数据可退回 — 该图片将不可用 */
            continue;
        }

        /* 分块读取: 256B 栈内缓冲 → 32 位拷贝到外部 SRAM */
        uint8_t chunk[256];
        uint32_t remain = sz;
        uint32_t off    = 0;
        while (remain > 0) {
            uint32_t n = (remain > 256) ? 256 : remain;
            lfs_ssize_t rd = lfs_file_read(&g_lfs, &f, chunk, n);
            if (rd <= 0) break;
            uint32_t *s32 = (uint32_t *)chunk;
            uint32_t *d32 = (uint32_t *)(buf + off);
            for (uint32_t j = 0; j < ((uint32_t)rd + 3) / 4; j++)
                d32[j] = s32[j];
            off    += (uint32_t)rd;
            remain -= (uint32_t)rd;
        }
        lfs_file_close(&g_lfs, &f);

        /* 填充描述符 */
        lv_img_dsc_t *d = e->dsc;
        d->header.always_zero = 0;
        d->header.w  = e->w;
        d->header.h  = e->h;
        d->header.cf = e->cf;
        d->data_size = sz;
        d->data      = buf;
        *s_sram_dscs[i] = *d;

        buf += aligned;
        total += aligned;
        loaded_count++;
    }

    printf("[IMG] Loaded %lu bytes to SRAM (%u/6 images)\n",
           total, (unsigned)loaded_count);

    if (loaded_count == 0) {
        printf("[IMG] WARN: No images loaded from LFS\n");
        s_src = 0;
    }
}
