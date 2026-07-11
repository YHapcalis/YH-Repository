/*
 * spi_img_loader.h — SPI Flash → 外部 SRAM 图片预加载
 *
 * 在 LVGL 初始化后调用 spi_img_load_all()，将界面图片
 * 从 SPI Flash 读到 FSMC 外部 SRAM (0x68000000)。
 */

#ifndef SPI_IMG_LOADER_H
#define SPI_IMG_LOADER_H

#include "lvgl.h"

/* 所有图片的 SRAM 版描述符（加载后可用） */
extern lv_img_dsc_t img_sram_gauge_bg;
extern lv_img_dsc_t img_sram_needle;
extern lv_img_dsc_t img_sram_light;
extern lv_img_dsc_t img_sram_watertemp;
extern lv_img_dsc_t img_sram_turnlight;
extern lv_img_dsc_t img_sram_safetybelt;

/* 从 SPI Flash 加载全部图片到 0x68000000 */
void spi_img_load_all(void);

/* 返回图片数据源: 0=编译期内部Flash, 1=SPI Flash→外部SRAM */
uint8_t spi_img_get_source(void);

/* 返回后备数据是否仍在启用: 0=已释放(内部Flash图片空间可复用于其他功能), 1=仍在用 */
uint8_t spi_img_fallback_active(void);

#endif /* SPI_IMG_LOADER_H */
