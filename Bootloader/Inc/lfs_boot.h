/*
 * lfs_boot.h — Bootloader 侧 LittleFS 最小移植（只读）
 *
 * Bootloader 只需要从 LFS 读取 firmware.bak 文件来恢复固件。
 * LFS_READONLY 模式下编译 lfs.c，不包含写入/格式化功能。
 *
 * 依赖：EN25Q128_Init() 必须在调用 lfs_boot_init() 前完成。
 */

#ifndef LFS_BOOT_H
#define LFS_BOOT_H

#include "lfs.h"

/* 全局 LFS 对象 */
extern lfs_t g_lfs;

/*
 * 尝试挂载 LittleFS
 * @return 0=成功, 负值=失败
 */
int lfs_boot_init(void);

#endif /* LFS_BOOT_H */
