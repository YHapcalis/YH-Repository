/**
 * @file    lv_fs_spi_flash.h
 * @brief   LVGL v9.3 文件系统驱动 — W25Q128 SPI Flash
 *
 * 路径格式: "S:<offset>:<size>"
 *   例:   "S:00000000:00096000" → offset=0x00000000, size=0x00096000
 */

#ifndef __LV_FS_SPI_FLASH_H
#define __LV_FS_SPI_FLASH_H

#include "lvgl.h"

/** 注册 SPI Flash FS 驱动为 "S:" 盘符 */
void lv_fs_spi_flash_init(void);

#endif /* __LV_FS_SPI_FLASH_H */
