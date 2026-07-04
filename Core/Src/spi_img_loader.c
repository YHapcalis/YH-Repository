/**
 * @file    spi_img_loader.c
 * @brief   SPI Flash (LittleFS) → 外部 SRAM 图片预加载
 *
 * 6 张图片存放于 SPI Flash 的 LittleFS 文件系统中，启动时由 LFS
 * 读取到外部 SRAM (FSMC @ 0x68000000)，LVGL 直接从 SRAM 渲染。
 *
 * 首次启动（LFS 无图片文件）：从编译期后备数据自动创建文件。
 * 后备机制在文件系统稳定运行后可移除，释放 ~57KB 内部 Flash。
 *
 * LFS 文件名:
 *   gauge.bin      – gauge-bg     (233×233 INDEXED_4BIT)
 *   needle.bin     – needle       (22×86  TRUE_COLOR_ALPHA)
 *   light.bin      – light        (32×32  TRUE_COLOR_ALPHA)
 *   watertemp.bin  – watertemp    (32×32  TRUE_COLOR_ALPHA)
 *   turnlight.bin  – turnlight    (32×32  TRUE_COLOR_ALPHA)
 *   safetybelt.bin – safetybelt   (32×32  TRUE_COLOR_ALPHA)
 */

#include "spi_img_loader.h"
#include "en25q128.h"
#include "lfs_port.h"
#include <string.h>
#include <stdio.h>

#define SRAM_BASE    ((uint8_t *)0x68000000UL)

/* ── 编译期后备图片 ── */
extern const lv_img_dsc_t ui_img_942215904;
extern const lv_img_dsc_t ui_img_1601502596;
extern const lv_img_dsc_t ui_img_light_png;
extern const lv_img_dsc_t ui_img_temp_gray_png;
extern const lv_img_dsc_t ui_img_turn_light_png;
extern const lv_img_dsc_t ui_img_safety_belt_png;

static const lv_img_dsc_t *s_fallback[] = {
    &ui_img_942215904,      /* gauge.bin   */
    &ui_img_1601502596,     /* needle.bin  */
    &ui_img_light_png,      /* light.bin   */
    &ui_img_temp_gray_png,  /* watertemp.bin */
    &ui_img_turn_light_png, /* turnlight.bin */
    &ui_img_safety_belt_png,/* safetybelt.bin */
};

/* 图片元数据表 — 使用 LFS 文件名替代 SPI Flash 偏移 */
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

/* 数据源: 0=内部Flash, 1=SPI→SRAM */
static uint8_t s_src = 0;

uint8_t spi_img_get_source(void) { return s_src; }

/* 导出指针数组（供 app_ui.c 方便索引） */
static lv_img_dsc_t *s_sram_dscs[] = {
    &img_sram_gauge_bg, &img_sram_needle,
    &img_sram_light,    &img_sram_watertemp,
    &img_sram_turnlight, &img_sram_safetybelt,
};

static int  lfs_write_images_from_fallback(void);
static void spi_load_to_ram(uint8_t *buf);

/* ═════════════════════════════════════════════════════════════
 *  从后备数据写入 LFS（首次启动 / 文件缺失时调用）
 * ═════════════════════════════════════════════════════════════ */
static int lfs_write_images_from_fallback(void)
{
    printf("[IMG] Writing 6 images to LFS from fallback...\n");

    for (int i = 0; i < 6; i++) {
        lfs_file_t f;
        int err = lfs_file_open(&g_lfs, &f, s_imgs[i].filename,
                                LFS_O_WRONLY | LFS_O_CREAT);
        if (err) {
            printf("[IMG]  FAIL open %s: %d\n", s_imgs[i].filename, err);
            return err;
        }

        lfs_ssize_t written = lfs_file_write(&g_lfs, &f,
                                             s_fallback[i]->data,
                                             s_imgs[i].data_size);
        if (written != (lfs_ssize_t)s_imgs[i].data_size) {
            printf("[IMG]  FAIL write %s: %d\n", s_imgs[i].filename,
                   (int)written);
            lfs_file_close(&g_lfs, &f);
            return (int)written;
        }

        lfs_file_close(&g_lfs, &f);
        printf("[IMG]  [%d] %s -> %lu B\n", i, s_imgs[i].filename,
               (unsigned long)s_imgs[i].data_size);
    }

    printf("[IMG] LFS write done.\n");
    return 0;
}

/* ═════════════════════════════════════════════════════════════
 *  加载全部图片
 * ═════════════════════════════════════════════════════════════ */
void spi_img_load_all(void)
{
    /* ---- 检查 LFS 中是否有图片文件 ---- */
    lfs_file_t probe;
    int need_write = 0;
    int err = lfs_file_open(&g_lfs, &probe, "gauge.bin", LFS_O_RDONLY);
    if (err) {
        printf("[IMG] gauge.bin not found on LFS, writing...\n");
        need_write = 1;
    } else {
        lfs_file_close(&g_lfs, &probe);
    }

    if (need_write) {
        err = lfs_write_images_from_fallback();
        if (err) {
            printf("[IMG] LFS write FAILED (%d), using compiled-in images\n",
                   err);
            for (int i = 0; i < 6; i++)
                *s_sram_dscs[i] = *s_fallback[i];
            return;
        }
    }

    /* ---- 测试外部 SRAM ---- */
    volatile uint32_t *st = (volatile uint32_t *)0x68000000;
    uint32_t orig = st[0];
    st[0] = 0x12345678;
    if (st[0] != 0x12345678) {
        st[0] = orig;
        printf("[IMG] SRAM FAIL, using compiled-in images\n");
        for (int i = 0; i < 6; i++) *s_sram_dscs[i] = *s_fallback[i];
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

    for (int i = 0; i < 6; i++) {
        const img_entry_t *e = &s_imgs[i];
        uint32_t sz     = e->data_size;
        uint32_t aligned = (sz + 3) & ~3;

        /* 打开 LFS 文件 */
        lfs_file_t f;
        int err = lfs_file_open(&g_lfs, &f, e->filename, LFS_O_RDONLY);
        if (err) {
            printf("[IMG] FAIL open %s: %d, fallback\n", e->filename, err);
            *s_sram_dscs[i] = *s_fallback[i];
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
    }
    printf("[IMG] Loaded %lu bytes to SRAM\n", total);
}
