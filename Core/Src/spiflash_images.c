/**
 * @file    spiflash_images.c
 * @brief   小图预加载到外部 SRAM (≤256KB 安全边界)
 */

#include "spiflash_images.h"
#include "en25q128.h"
#include "lvgl.h"
#include "spiflash_offset_table.h"
#include <stdio.h>

#define SRAM_IMG_BASE  0x68000000UL
#define HDR_SZ         12

/* ---- 图片描述符 ---- */
static lv_image_dsc_t _desc_kmbg        = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=166,.h=166,.stride=332,.reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_battery_bak = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=43, .h=24, .stride=86, .reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_battery_ind = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=43, .h=24, .stride=86, .reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_direction   = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=50, .h=42, .stride=100,.reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_light       = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=46, .h=46, .stride=92, .reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_high_beam   = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=46, .h=46, .stride=92, .reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_home        = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=43, .h=43, .stride=86, .reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_logo        = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=100,.h=35, .stride=200,.reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_monitor     = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=41, .h=37, .stride=82, .reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_nav         = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=116,.h=116,.stride=232,.reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_temper      = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=91, .h=123,.stride=182,.reserved_2=0 },.data_size=0,.data=NULL };
static lv_image_dsc_t _desc_weather     = { .header = { .magic=0x19,.cf=0x14,.flags=0,.w=66, .h=66, .stride=132,.reserved_2=0 },.data_size=0,.data=NULL };

const lv_image_dsc_t *img_kmbg        = &_desc_kmbg;
const lv_image_dsc_t *img_battery_bak = &_desc_battery_bak;
const lv_image_dsc_t *img_battery_ind = &_desc_battery_ind;
const lv_image_dsc_t *img_direction   = &_desc_direction;
const lv_image_dsc_t *img_light       = &_desc_light;
const lv_image_dsc_t *img_high_beam   = &_desc_high_beam;
const lv_image_dsc_t *img_home        = &_desc_home;
const lv_image_dsc_t *img_logo        = &_desc_logo;
const lv_image_dsc_t *img_monitor     = &_desc_monitor;
const lv_image_dsc_t *img_nav         = &_desc_nav;
const lv_image_dsc_t *img_temper      = &_desc_temper;
const lv_image_dsc_t *img_weather     = &_desc_weather;

static void load_one(lv_image_dsc_t *desc, uint32_t offset, uint32_t size,
                     uint8_t **sram_ptr)
{
    if (size <= HDR_SZ) return;
    uint32_t px_sz = size - HDR_SZ;
    uint8_t *buf = *sram_ptr;
    EN25Q128_Read((volatile uint8_t *)buf, offset + HDR_SZ, px_sz);
    desc->data = buf;
    desc->data_size = px_sz;
    *sram_ptr += (px_sz + 3) & ~3;
}

void spiflash_images_init(void)
{
    __disable_irq();
    uint8_t *sram = (uint8_t *)SRAM_IMG_BASE;

    load_one(&_desc_kmbg,        OFFSET__KMBG_RGB565A8_166X166_MAP,       SIZE__KMBG_RGB565A8_166X166_MAP, &sram);
    load_one(&_desc_battery_bak, OFFSET__BATTERY_BAK_RGB565A8_43X24_MAP,   SIZE__BATTERY_BAK_RGB565A8_43X24_MAP, &sram);
    load_one(&_desc_battery_ind, OFFSET__BATTERY_IND_RGB565A8_43X24_MAP,   SIZE__BATTERY_IND_RGB565A8_43X24_MAP, &sram);
    load_one(&_desc_direction,   OFFSET__DIRECTION_RGB565A8_50X42_MAP,     SIZE__DIRECTION_RGB565A8_50X42_MAP, &sram);
    load_one(&_desc_light,       OFFSET__LIGHT_RGB565A8_46X46_MAP,         SIZE__LIGHT_RGB565A8_46X46_MAP, &sram);
    load_one(&_desc_high_beam,   OFFSET__HIGH_BEAM_RGB565A8_46X46_MAP,     SIZE__HIGH_BEAM_RGB565A8_46X46_MAP, &sram);
    load_one(&_desc_home,        OFFSET__HOME_RGB565A8_43X43_MAP,          SIZE__HOME_RGB565A8_43X43_MAP, &sram);
    load_one(&_desc_logo,        OFFSET__LOGO_RGB565A8_100X35_MAP,         SIZE__LOGO_RGB565A8_100X35_MAP, &sram);
    load_one(&_desc_monitor,     OFFSET__MONITOR_RGB565A8_41X37_MAP,       SIZE__MONITOR_RGB565A8_41X37_MAP, &sram);
    load_one(&_desc_nav,         OFFSET__IMG_NAV_1_RGB565A8_116X116_MAP,   SIZE__IMG_NAV_1_RGB565A8_116X116_MAP, &sram);
    load_one(&_desc_temper,      OFFSET__TEM_RGB565A8_91X123_MAP,          SIZE__TEM_RGB565A8_91X123_MAP, &sram);
    load_one(&_desc_weather,     OFFSET__SUNNY_RGB565A8_66X66_MAP,         SIZE__SUNNY_RGB565A8_66X66_MAP, &sram);

    printf("[IMG] SRAM: %luKB loaded\r\n",
           (unsigned long)((uintptr_t)sram - SRAM_IMG_BASE) / 1024);
    __enable_irq();
}
