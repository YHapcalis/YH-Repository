/**
 * @file    lv_fs_spi_flash.c
 * @brief   LVGL v9.3 FS 驱动 — W25Q128 SPI Flash
 *
 * 路径 "S:<offset>:<size>" — offset/size 为 8 位十六进制, 无 0x 前缀
 */

#include "lv_fs_spi_flash.h"
#include "en25q128.h"
#include <string.h>
#include <stdio.h>

/* ---- 文件私有数据 ---- */
typedef struct {
    uint32_t offset;    /* SPI Flash 字节偏移 */
    uint32_t size;      /* 文件大小 */
    uint32_t pos;       /* 当前读位置 */
} spiflash_file_t;

/* ---- open: 返回指向 spiflash_file_t 的指针, 失败返回 NULL ---- */
static void *spiflash_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    if (mode != LV_FS_MODE_RD) return NULL;

    /* 解析 "XXXXXXXX:YYYYYYYY" — LVGL v9.3 的 lv_fs_resolve_path() 已剥掉 "S:" */
    uint32_t off = 0, sz = 0;
    if (sscanf(path, "%8x:%8x", &off, &sz) < 2) return NULL;
    if (sz == 0) return NULL;

    /* 16MB 边界检查 */
    if ((uint64_t)off + sz > 0x1000000ULL) return NULL;

    spiflash_file_t *sf = lv_malloc(sizeof(spiflash_file_t));
    if (sf == NULL) return NULL;

    sf->offset = off;
    sf->size   = sz;
    sf->pos    = 0;
    return sf;
}

/* ---- close ---- */
static lv_fs_res_t spiflash_close(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    spiflash_file_t *sf = (spiflash_file_t *)file_p;
    if (sf) lv_free(sf);
    return LV_FS_RES_OK;
}

/* ---- read ---- */
static lv_fs_res_t spiflash_read(lv_fs_drv_t *drv, void *file_p,
                                  void *buf, uint32_t btr, uint32_t *br)
{
    (void)drv;
    spiflash_file_t *sf = (spiflash_file_t *)file_p;
    if (sf == NULL) return LV_FS_RES_INV_PARAM;

    uint32_t remain = sf->size - sf->pos;
    uint32_t actual = (btr < remain) ? btr : remain;
    if (actual == 0) {
        if (br) *br = 0;
        return LV_FS_RES_OK;
    }

    EN25Q128_Read((volatile uint8_t *)buf, sf->offset + sf->pos, actual);
    sf->pos += actual;
    if (br) *br = actual;
    return LV_FS_RES_OK;
}

/* ---- seek (v9.3: pos + whence) ---- */
static lv_fs_res_t spiflash_seek(lv_fs_drv_t *drv, void *file_p,
                                  uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;
    spiflash_file_t *sf = (spiflash_file_t *)file_p;
    if (sf == NULL) return LV_FS_RES_INV_PARAM;

    uint32_t new_pos;
    switch (whence) {
    case LV_FS_SEEK_SET: new_pos = pos; break;
    case LV_FS_SEEK_CUR: new_pos = sf->pos + pos; break;
    case LV_FS_SEEK_END: new_pos = sf->size + pos; break;
    default: return LV_FS_RES_INV_PARAM;
    }

    if (new_pos > sf->size) return LV_FS_RES_INV_PARAM;
    sf->pos = new_pos;
    return LV_FS_RES_OK;
}

/* ---- tell ---- */
static lv_fs_res_t spiflash_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    spiflash_file_t *sf = (spiflash_file_t *)file_p;
    if (sf == NULL) return LV_FS_RES_INV_PARAM;
    *pos_p = sf->pos;
    return LV_FS_RES_OK;
}

/* ---- 注册 ---- */
void lv_fs_spi_flash_init(void)
{
    static lv_fs_drv_t drv;
    lv_fs_drv_init(&drv);

    drv.letter     = 'S';
    drv.cache_size = 0;
    drv.open_cb    = spiflash_open;
    drv.close_cb   = spiflash_close;
    drv.read_cb    = spiflash_read;
    drv.write_cb   = NULL;           /* 只读 */
    drv.seek_cb    = spiflash_seek;
    drv.tell_cb    = spiflash_tell;

    lv_fs_drv_register(&drv);
}
