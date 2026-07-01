/*
 * inter_flash_cfg.h — 参数扇区管理（Magic + ota_flag + Checksum）
 *
 * 参数存储在 Flash Sector 11 (0x080E0000, 128KB)
 * struct 强制 1 字节对齐，大小必须是 4 的倍数
 */

#ifndef __INTER_FLASH_CFG_H__
#define __INTER_FLASH_CFG_H__

#include <stdint.h>

#pragma pack(1)
typedef struct {
    uint8_t magic[4];           /* 0xAA, 0xBB, 0xCC, 0xDD */
    uint32_t ota_bin_version;   /* 固件版本号 */
    uint8_t ota_flag;           /* 0=正常启动, 1=需要 OTA */
    uint8_t checksum;           /* 累加和校验（覆盖前面 9 字节） */
    uint8_t format[2];          /* 占位对齐到 12 字节 */
} inter_flash_cfg_param_typeDef;
#pragma pack()

/* ── 接口 ── */
uint8_t  inter_flash_cfg_init(void);
int8_t   inter_flash_cfg_get_app_update_flag(void);
uint8_t  inter_flash_cfg_set_app_update_flag(uint8_t flag);

#endif /* __INTER_FLASH_CFG_H__ */
