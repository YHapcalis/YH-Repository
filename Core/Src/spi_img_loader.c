/**
 * @file    spi_img_loader.c
 * @brief   SPI Flash (LittleFS) → 外部 SRAM 图片预加载
 *
 * 三级状态机:
 *   [正常]  LFS → 外部 SRAM → LVGL (内部 Flash 备份区擦除)
 *   [降级]  LFS → 内部 Flash 扇区9 → LVGL (SRAM 故障时)
 *   [抢救]  编译后备 → LVGL (SPI Flash 和 SRAM 都故障)
 *
 * 首次启动从编译期后备写入 LFS，后续状态自动切换。
 *
 * LFS 文件名:
 *   gauge.bin      – gauge-bg     (233×233 INDEXED_4BIT, 27325B)
 *   needle.bin     – needle       (22×86  TRUE_COLOR_ALPHA, 5676B)
 *   light.bin      – light        (32×32  TRUE_COLOR_ALPHA, 3072B)
 *   watertemp.bin  – watertemp    (32×32  TRUE_COLOR_ALPHA, 3072B)
 *   turnlight.bin  – turnlight    (32×32  TRUE_COLOR_ALPHA, 3072B)
 *   safetybelt.bin – safetybelt   (32×32  TRUE_COLOR_ALPHA, 3072B)
 *   home.bin       – (已移除，之前为 160×160 TRUE_COLOR_ALPHA 测试用)
 */

#include "spi_img_loader.h"
#include "en25q128.h"
#include "lfs_port.h"
#include "stm32f4xx_hal_flash.h"
#include <string.h>
#include <stdio.h>

#define SRAM_BASE          ((uint8_t *)0x68000000UL)

/* ── 运行时备份区：内部 Flash 扇区9 (0x080A0000, 128KB) ── */
/* 所有图片数据按对齐字节连续存放，与 s_imgs[] 顺序一致 */
#define BACKUP_FLASH_ADDR  0x080A0000U
#define BACKUP_FLASH_SECTOR FLASH_SECTOR_9
#define BACKUP_MAX_SIZE     (128UL * 1024UL)  /* 128KB */

/* ── 编译期后备图片 ── */
extern const lv_img_dsc_t ui_img_942215904;
extern const lv_img_dsc_t ui_img_1601502596;
extern const lv_img_dsc_t ui_img_light_png;
extern const lv_img_dsc_t ui_img_temp_gray_png;
extern const lv_img_dsc_t ui_img_turn_light_png;
extern const lv_img_dsc_t ui_img_safety_belt_png;

#define FALLBACK_COUNT 6   /* 有后备数据的图片数 */
static const lv_img_dsc_t *s_fallback[] = {
    &ui_img_942215904,      /* gauge.bin   */
    &ui_img_1601502596,     /* needle.bin  */
    &ui_img_light_png,      /* light.bin   */
    &ui_img_temp_gray_png,  /* watertemp.bin */
    &ui_img_turn_light_png, /* turnlight.bin */
    &ui_img_safety_belt_png,/* safetybelt.bin */
};

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

#define IMG_COUNT 6
static const img_entry_t s_imgs[] = {
    {"gauge.bin",     27325, 233, 233, LV_IMG_CF_INDEXED_4BIT,       &s_dsc_gb},
    {"needle.bin",     5676,  22,  86, LV_IMG_CF_TRUE_COLOR_ALPHA,   &s_dsc_nd},
    {"light.bin",      3072,  32,  32, LV_IMG_CF_TRUE_COLOR_ALPHA,   &s_dsc_lt},
    {"watertemp.bin",  3072,  32,  32, LV_IMG_CF_TRUE_COLOR_ALPHA,   &s_dsc_wt},
    {"turnlight.bin",  3072,  32,  32, LV_IMG_CF_TRUE_COLOR_ALPHA,   &s_dsc_tl},
    {"safetybelt.bin", 3072,  32,  32, LV_IMG_CF_TRUE_COLOR_ALPHA,   &s_dsc_sb},
};

/* 数据源: 0=内部Flash(后备), 1=SPI→SRAM */
static uint8_t s_src = 0;
static uint8_t s_fallback_active = 1;  /* 1=后备数据仍在使用 */

uint8_t spi_img_get_source(void) { return s_src; }
uint8_t spi_img_fallback_active(void) { return s_fallback_active; }

/* 导出指针数组（供 app_ui.c 方便索引） */
static lv_img_dsc_t *s_sram_dscs[] = {
    &img_sram_gauge_bg, &img_sram_needle,
    &img_sram_light,    &img_sram_watertemp,
    &img_sram_turnlight, &img_sram_safetybelt,
};

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int  lfs_write_images_from_fallback(void);
static void spi_load_to_ram(uint8_t *buf);

/* ═════════════════════════════════════════════════════════════
 *  内部 Flash 运行时备份区（扇区9）操作
 * ═════════════════════════════════════════════════════════════ */
#define BACKUP_MAGIC  0x494D474D  /* "IMGM" — 标记备份区有效 */

/* 擦除备份扇区（释放空间给其他功能） */
static void backup_erase(void)
{
    HAL_FLASH_Unlock();
    FLASH_Erase_Sector(BACKUP_FLASH_SECTOR, VOLTAGE_RANGE_3);
    HAL_FLASH_Lock();
}

/* 将所有图片从 LFS 写入备份扇区 */
static int backup_write_all_from_lfs(void)
{
    uint32_t addr = BACKUP_FLASH_ADDR;
    uint32_t total = 0;
    uint8_t chunk[256];

    backup_erase();
    HAL_FLASH_Unlock();

    uint32_t magic = BACKUP_MAGIC;
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, magic);
    addr += 4;

    for (int i = 0; i < IMG_COUNT; i++) {
        const img_entry_t *e = &s_imgs[i];
        uint32_t sz = e->data_size;

        lfs_file_t f;
        if (lfs_file_open(&g_lfs, &f, e->filename, LFS_O_RDONLY)) {
            printf("[IMG] BACKUP: can't open %s\n", e->filename);
            HAL_FLASH_Lock(); return -1;
        }

        uint32_t remain = sz;
        while (remain > 0) {
            uint32_t n = (remain > 256) ? 256 : remain;
            lfs_ssize_t rd = lfs_file_read(&g_lfs, &f, chunk, n);
            if (rd <= 0) break;
            for (int32_t j = 0; j < rd; j += 4)
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + j,
                                  *(uint32_t *)(chunk + j));
            addr += (uint32_t)rd;
            remain -= (uint32_t)rd;
        }
        lfs_file_close(&g_lfs, &f);
        total += (sz + 3) & ~3;
    }
    HAL_FLASH_Lock();

    printf("[IMG] Backup written to 0x%08lX (%lu bytes)\n",
           (unsigned long)BACKUP_FLASH_ADDR, (unsigned long)total);
    return 0;
}

/* 检查备份扇区是否有效 */
static int backup_is_valid(void)
{
    return (*(volatile uint32_t *)BACKUP_FLASH_ADDR == BACKUP_MAGIC);
}

/* ═════════════════════════════════════════════════════════════
 *  从后备数据写入 LFS（首次启动 / 文件缺失时调用）
 * ═════════════════════════════════════════════════════════════ */
static int lfs_write_images_from_fallback(void)
{
    printf("[IMG] Writing images to LFS from fallback...\n");
    int written_count = 0;

    for (int i = 0; i < FALLBACK_COUNT; i++) {
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
        written_count++;
    }

    printf("[IMG] LFS write done (%u images)\n", written_count);
    return 0;
}

/* ═════════════════════════════════════════════════════════════
 *  加载全部图片
 * ═════════════════════════════════════════════════════════════ */
void spi_img_load_all(void)
{
    /* ═══════════════════════════════════════════════════════════
     *  阶段1: 确保 LFS 图片就绪（缺失的从编译后备写入）
     * ═══════════════════════════════════════════════════════════ */
    for (int i = 0; i < FALLBACK_COUNT; i++) {
        lfs_file_t f;
        int err = lfs_file_open(&g_lfs, &f, s_imgs[i].filename, LFS_O_RDONLY);
        if (err) {
            printf("[IMG] %s not found, writing from fallback...\n",
                   s_imgs[i].filename);
        } else {
            lfs_size_t fsize = lfs_file_size(&g_lfs, &f);
            lfs_file_close(&g_lfs, &f);
            if (fsize == s_imgs[i].data_size) continue;  /* 正常 */
            printf("[IMG] %s size mismatch (%u vs %u), rewriting...\n",
                   s_imgs[i].filename, (unsigned)fsize,
                   (unsigned)s_imgs[i].data_size);
            lfs_remove(&g_lfs, s_imgs[i].filename);
        }

        err = lfs_file_open(&g_lfs, &f, s_imgs[i].filename,
                            LFS_O_WRONLY | LFS_O_CREAT);
        if (err) continue;
        lfs_ssize_t w = lfs_file_write(&g_lfs, &f, s_fallback[i]->data,
                                        s_imgs[i].data_size);
        lfs_file_close(&g_lfs, &f);
        if (w == (lfs_ssize_t)s_imgs[i].data_size)
            printf("[IMG]  wrote %s (%lu B)\n",
                   s_imgs[i].filename, (unsigned long)w);
    }

    /* ═══════════════════════════════════════════════════════════
     *  阶段2: 测试外部 SRAM
     * ═══════════════════════════════════════════════════════════ */
    volatile uint32_t *st = (volatile uint32_t *)0x68000000;
    uint32_t orig = st[0];
    st[0] = 0x12345678;
    int sram_ok = (st[0] == 0x12345678);
    st[0] = orig;

    if (sram_ok) {
        /* ── 状态 A: SRAM 和 LFS 正常 → 加载到 SRAM，擦除备份区 ── */
        printf("[IMG] Loading images from LFS to SRAM...\n");
        s_src = 1;
        spi_load_to_ram(SRAM_BASE);

        /* 正常运转 → 释放内部 Flash 备份区（擦除扇区9） */
        if (backup_is_valid()) {
            backup_erase();
            printf("[IMG] Backup sector freed\n");
        }
        s_fallback_active = 0;
        return;
    }

    /* ═══════════════════════════════════════════════════════════
     *  阶段3: SRAM 故障 → 使用内部 Flash 备份区
     * ═══════════════════════════════════════════════════════════ */
    printf("[IMG] SRAM FAIL, switching to Flash backup\n");

    if (backup_is_valid()) {
        /* ── 状态 B: 已有有效备份 → 直接使用 ── */
        uint32_t addr = BACKUP_FLASH_ADDR + 4;
        for (int i = 0; i < IMG_COUNT; i++) {
            const img_entry_t *e = &s_imgs[i];
            uint32_t aligned = (e->data_size + 3) & ~3;
            lv_img_dsc_t d;
            d.header.always_zero = 0;
            d.header.w  = e->w;
            d.header.h  = e->h;
            d.header.cf = e->cf;
            d.data_size = e->data_size;
            d.data      = (const uint8_t *)addr;
            *e->dsc = d;
            *s_sram_dscs[i] = d;
            addr += aligned;
        }
        printf("[IMG] Using Flash backup (internal)\n");
        s_fallback_active = 1;
        return;
    }

    /* ── 状态 C: 无备份 → 从 LFS 创建备份 ── */
    if (backup_write_all_from_lfs() == 0) {
        uint32_t addr = BACKUP_FLASH_ADDR + 4;
        for (int i = 0; i < IMG_COUNT; i++) {
            const img_entry_t *e = &s_imgs[i];
            uint32_t aligned = (e->data_size + 3) & ~3;
            lv_img_dsc_t d;
            d.header.always_zero = 0;
            d.header.w  = e->w;
            d.header.h  = e->h;
            d.header.cf = e->cf;
            d.data_size = e->data_size;
            d.data      = (const uint8_t *)addr;
            *e->dsc = d;
            *s_sram_dscs[i] = d;
            addr += aligned;
        }
        s_fallback_active = 1;
        return;
    }

    /* ── 状态 D: SPI Flash 也故障 → 使用编译期后备（抢救） ── */
    printf("[IMG] SPI Flash backup failed, using compile fallback\n");
    for (int i = 0; i < FALLBACK_COUNT; i++)
        *s_sram_dscs[i] = *s_fallback[i];
    s_fallback_active = 1;
}

static void spi_load_to_ram(uint8_t *buf)
{
    uint32_t total = 0;
    uint32_t loaded_count = 0;

    for (int i = 0; i < IMG_COUNT; i++) {
        const img_entry_t *e = &s_imgs[i];
        uint32_t sz     = e->data_size;
        uint32_t aligned = (sz + 3) & ~3;

        /* 打开 LFS 文件 */
        lfs_file_t f;
        int err = lfs_file_open(&g_lfs, &f, e->filename, LFS_O_RDONLY);
        if (err) {
            /* LFS 无此文件 — 尝试内部 Flash 备份区或编译后备 */
            if (backup_is_valid()) {
                /* 从备份区读取：跳过魔数(4B)，按对齐偏移定位 */
                uint32_t off = 4;
                for (int j = 0; j < i; j++)
                    off += (s_imgs[j].data_size + 3) & ~3;
                lv_img_dsc_t d;
                d.header.always_zero = 0;
                d.header.w  = e->w;
                d.header.h  = e->h;
                d.header.cf = e->cf;
                d.data_size = e->data_size;
                d.data      = (const uint8_t *)(BACKUP_FLASH_ADDR + off);
                *e->dsc = d;
                *s_sram_dscs[i] = d;
                printf("[IMG] %s from Flash backup\n", e->filename);
                loaded_count++;
                continue;
            }
            if (i < FALLBACK_COUNT) {
                printf("[IMG] %s not on LFS, using compile fallback\n",
                       e->filename);
                *s_sram_dscs[i] = *s_fallback[i];
                loaded_count++;
                continue;
            }
            printf("[IMG] WARN: %s not found, no fallback (%d)\n",
                   e->filename, err);
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

    printf("[IMG] Loaded %lu bytes to SRAM (%u/%u images)\n",
           total, (unsigned)loaded_count, (unsigned)IMG_COUNT);

    if (loaded_count == 0) {
        printf("[IMG] WARN: No images loaded from LFS\n");
        s_src = 0;
    }
}
