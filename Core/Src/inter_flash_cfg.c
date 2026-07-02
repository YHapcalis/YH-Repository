/*
 * inter_flash_cfg.c — 参数扇区管理
 *
 * 读取 → 校验 Magic + Checksum → 返回 ota_flag
 * 写入 → 更新 flag → 重算 Checksum → 擦除扇区 → 写入
 *
 * 从 uint3code 例程 34Bootloader 移植，适配 F407 sector 擦除
 */

#include "inter_flash_cfg.h"
#include "inter_flashif.h"
#include <stdio.h>

static inter_flash_cfg_param_typeDef s_flash_cfg;

/* ═══════════════════════ 从 Flash 读取并校验参数 ═══════════════════════ */

static uint8_t inter_flash_cfg_load(void)
{
    /* 从 Flash 读到内存 */
    inter_flashif_read(INTER_FLASH_PARAM_ADDR,
                       (uint8_t *)&s_flash_cfg, sizeof(s_flash_cfg));

    /* 校验 Magic Number */
    if (s_flash_cfg.magic[0] != 0xAA || s_flash_cfg.magic[1] != 0xBB ||
        s_flash_cfg.magic[2] != 0xCC || s_flash_cfg.magic[3] != 0xDD) {
        printf("[CFG] Magic error (got %02X%02X%02X%02X)\r\n",
               s_flash_cfg.magic[0], s_flash_cfg.magic[1],
               s_flash_cfg.magic[2], s_flash_cfg.magic[3]);
        return 1;   /* Magic 错误 */
    }

    /* 校验 Checksum: 累加 magic + version + flag 共 9 字节 */
    uint8_t calc = inter_flash_checksum((uint8_t *)&s_flash_cfg,
                                        sizeof(s_flash_cfg) - 3);
    if (calc != s_flash_cfg.checksum) {
        printf("[CFG] Checksum error (calc=0x%02X, stored=0x%02X)\r\n",
               calc, s_flash_cfg.checksum);
        return 2;   /* Checksum 错误 */
    }

    return 0;       /* 校验通过 */
}

/* ═══════════════════════ 初始化（验证参数扇区完整性） ═══════════════════════ */

uint8_t inter_flash_cfg_init(void)
{
    /* 检查 struct 大小是否对齐到 4 的倍数 */
    if (sizeof(inter_flash_cfg_param_typeDef) % 4 != 0) {
        printf("[CFG] ERROR: param struct size %zu not multiple of 4\r\n",
               sizeof(inter_flash_cfg_param_typeDef));
        while (1) {
            /* 死循环 — 编译期就应该修复 */
        }
    }

    uint8_t ret = inter_flash_cfg_load();
    if (ret != 0) {
        printf("[CFG] Parameter sector invalid (ret=%d), auto-initializing...\r\n", ret);
        inter_flash_cfg_set_app_update_flag(0);
        printf("[CFG] Auto-init done.\r\n");
    } else {
        printf("[CFG] Magic OK, ota_flag=%d\r\n", s_flash_cfg.ota_flag);
    }
    return 0;  /* 始终返回成功，上层不用再检查 */
}

/* ═══════════════════════ 读取 ota_flag ═══════════════════════ */

int8_t inter_flash_cfg_get_app_update_flag(void)
{
    uint8_t ret = inter_flash_cfg_load();
    if (ret != 0) return -1;    /* 参数无效 */
    return s_flash_cfg.ota_flag;
}

/* ═══════════════════════ 写入 ota_flag ═══════════════════════ */

uint8_t inter_flash_cfg_set_app_update_flag(uint8_t flag)
{
    /* 先读出当前值 */
    uint8_t ret = inter_flash_cfg_load();
    if (ret != 0) {
        /* 参数无效，用默认值初始化 */
        s_flash_cfg.magic[0] = 0xAA;
        s_flash_cfg.magic[1] = 0xBB;
        s_flash_cfg.magic[2] = 0xCC;
        s_flash_cfg.magic[3] = 0xDD;
        s_flash_cfg.ota_bin_version = 0;
        s_flash_cfg.ota_count[0] = 0;
        s_flash_cfg.ota_count[1] = 0;
    }

    s_flash_cfg.ota_flag = flag;

    /* 重算 Checksum */
    s_flash_cfg.checksum = inter_flash_checksum(
        (uint8_t *)&s_flash_cfg, sizeof(s_flash_cfg) - 3);

    /* 擦除参数扇区 + 写入 */
    ret = inter_flashif_erase_sector(INTER_FLASH_PARAM_ADDR);
    if (ret != 0) {
        printf("[CFG] Erase FAIL\r\n");
        return 1;
    }

    ret = inter_flashif_write(INTER_FLASH_PARAM_ADDR,
                              (uint32_t *)&s_flash_cfg,
                              sizeof(s_flash_cfg) / sizeof(uint32_t));
    if (ret != 0) {
        printf("[CFG] Write FAIL\r\n");
        return 2;
    }

    printf("[CFG] ota_flag set to %d (checksum=0x%02X)\r\n",
           flag, s_flash_cfg.checksum);
    return 0;
}

/* ═══════════════════════ OTA 计数 ═══════════════════════ */

uint16_t inter_flash_cfg_get_ota_count(void)
{
    uint8_t ret = inter_flash_cfg_load();
    if (ret != 0) return 0;
    return (uint16_t)(s_flash_cfg.ota_count[0]) |
           ((uint16_t)(s_flash_cfg.ota_count[1]) << 8);
}

void inter_flash_cfg_inc_ota_count(void)
{
    /* 先读出当前值 */
    uint8_t ret = inter_flash_cfg_load();
    if (ret != 0) return;

    uint16_t cnt = (uint16_t)(s_flash_cfg.ota_count[0]) |
                   ((uint16_t)(s_flash_cfg.ota_count[1]) << 8);
    cnt++;
    s_flash_cfg.ota_count[0] = cnt & 0xFF;
    s_flash_cfg.ota_count[1] = (cnt >> 8) & 0xFF;

    /* 擦除 + 写入（只写一次，不重算 checksum） */
    ret = inter_flashif_erase_sector(INTER_FLASH_PARAM_ADDR);
    if (ret != 0) return;
    inter_flashif_write(INTER_FLASH_PARAM_ADDR,
                        (uint32_t *)&s_flash_cfg,
                        sizeof(s_flash_cfg) / sizeof(uint32_t));
    printf("[CFG] OTA count incremented to %u\r\n", cnt);
}
