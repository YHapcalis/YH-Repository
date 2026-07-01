/*
 * boot_manager.c — Bootloader 跳转管理器实现
 *
 * 从 uint3code 例程 34Bootloader/boot_manager.c 移植
 * 适配 GCC (ARM GNU Toolchain)，使用 CMSIS __set_MSP() 而非 AC5 __asm
 *
 * 跳转流程：
 *   1. 检查 addr 处是否为合法栈顶指针（0x20000000 区域）
 *   2. 关全局中断
 *   3. 关 SysTick 并清 pending
 *   4. HAL_DeInit() 复位外设寄存器
 *   5. 设置 MSP = *(addr)
 *   6. PC = *(addr + 4) → 复位向量
 */

#include "boot_manager.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

typedef void (*APP_FUNC)(void);

uint8_t boot_check_stack2jump_app(uint32_t addr)
{
    APP_FUNC jump2app;

    /* ── 1. 检查栈顶指针合法性 ── */
    /* F407 SRAM 在 0x20000000 区域, _estack = 0x20000000 + 128K = 0x20020000 */
    /* 仅检查高 4 位是否为 0x2000（SRAM 空间），兼容不同 RAM 大小       */
    if (((*(__IO uint32_t *)addr) & 0xF0000000) != 0x20000000)
    {
        printf("[BOOT] ERROR: Invalid stack addr 0x%08lX at 0x%08lX\r\n",
               *(volatile uint32_t *)addr, addr);
        return 1;
    }

    /* ── 2. 关闭全局中断 (PRIMASK) ── */
    __set_PRIMASK(1);

    /* ── 3. 关闭 SysTick ── */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    /* 清除 SysTick 和 PendSV 的 pending 位 */
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    /* ── 4. 反初始化 HAL（关闭外设时钟和中断） ── */
    HAL_DeInit();

    /* ── 5. 设置主栈指针 ── */
    __set_MSP(*(volatile uint32_t *)addr);

    /* ── 6. 取复位向量 → 跳转 ── */
    jump2app = (APP_FUNC)(*(volatile uint32_t *)(addr + 4));
    jump2app();

    /* ── 不应到达此处 ── */
    printf("[BOOT] ERROR: jump failed\r\n");
    return 1;
}
