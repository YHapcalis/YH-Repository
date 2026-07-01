/*
 * boot_manager.h — Bootloader 跳转管理器
 *
 * 职责：
 *   验证 APP 向量表合法性 → 关闭中断 → 设置 MSP → 跳转
 *
 * 使用示例：
 *   boot_check_stack2jump_app(0x08008000);
 */

#ifndef __BOOT_MANAGER_H__
#define __BOOT_MANAGER_H__

#include <stdint.h>

/* ── 跳转到 APP ── */
/* addr: APP 起始地址（向量表基址），如 0x08008000 */
/* 成功不返回，失败返回 1 */
uint8_t boot_check_stack2jump_app(uint32_t addr);

#endif /* __BOOT_MANAGER_H__ */
