/*
 * lfs_port.h — LittleFS 底层驱动移植层
 *
 * 提供 LittleFS 所需的 4 个 HAL 函数 + 初始化接口。
 * 分区: SPI Flash 0x000000 ~ 0xDFFFFF (14MB, 3584 blocks × 4096)
 */

#ifndef LFS_PORT_H
#define LFS_PORT_H

#include "lfs.h"

/* 全局 LFS 对象（供 spi_img_loader 等模块使用） */
extern lfs_t g_lfs;

/*
 * 初始化 LittleFS 文件系统
 *
 * 尝试挂载，若失败（首次使用 / 损坏）则格式化后重新挂载。
 *
 * @return 0 = 成功, 负值 = LFS 错误码
 */
int lfs_storage_init(void);

#endif /* LFS_PORT_H */
