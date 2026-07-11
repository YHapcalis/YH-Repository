/**
 * sim_mode.h — 仿真模式编译开关
 *
 * 用法: CMakeLists.txt 已加 option(SIMULATION)，cmake -DSIMULATION=ON 启用
 *
 * 效果:
 *   - 自旋延时(NOP) → HAL_Delay (Renode TimeSkip 可跳过)
 *   - 缺少 Renode 模型的外设 (CAN/SPI) → HAL init 直接返回 OK
 *   - 大循环跳过 (色条填充)
 *   - 测试图案注入
 *
 * 在 Renode 仿真时生效，真实硬件不受影响。
 */

#ifndef __SIM_MODE_H
#define __SIM_MODE_H

#include "main.h"   /* HAL 类型 */
#include <stdio.h>

#ifdef SIMULATION

/* ================================================================
 * 仿真模式
 * ================================================================ */

/* ── 延时替换 ── */
#define SIM_DELAY_US(us)       do { if ((us) >= 1000) { HAL_Delay((us) / 1000); } else { HAL_Delay(1); } } while(0)
#define SIM_DELAY_MS(ms)       HAL_Delay(ms)

/* ── 大循环跳过 ── */
#define SIM_SKIP_LARGE_LOOP(count, label) \
    do { \
        printf("[SIM] Skip: %s (%u iters)\r\n", label, (unsigned)(count)); \
        HAL_Delay(10); \
    } while(0)

/* ── 测试图案注入 ── */
#define SIM_INJECT_VALUE(var, val)  do { (var) = (val); } while(0)

/* ═══════════════════════════════════════════════════════════════
 * 仿真模式 HAL 存根
 *
 * Renode 没有 CAN/SPI 等外设的完整寄存器模型。
 * HAL init 读寄存器返回值不符合预期 → 返回 HAL_ERROR。
 * 仿真模式下直接返回 HAL_OK，不执行实际初始化。
 *
 * 这些宏只在 SIMULATION 编译时替换 HAL 函数名。
 * 真实硬件编译时使用原本的 HAL 实现。
 * ═══════════════════════════════════════════════════════════════ */

/* CAN — Renode 无完整 CAN 模型 */
#undef  HAL_CAN_Init
#define HAL_CAN_Init(X)       ({ printf("[SIM] CAN_Init skipped\r\n"); HAL_OK; })

/* SPI — Renode SPI 模型不完整 */
#undef  HAL_SPI_Init
#define HAL_SPI_Init(X)       ({ printf("[SIM] SPI_Init skipped\r\n"); HAL_OK; })

/* ETH — 本板未连接 PHY */
#undef  HAL_ETH_Init
#define HAL_ETH_Init(X)       ({ printf("[SIM] ETH_Init skipped\r\n"); HAL_OK; })

/* ═══════════════════════════════════════════════════════════════
 * 仿真预初始化
 *
 * 在 main() 开头调用 SIM_Init()，配置所有外设的基本状态，
 * 让后续 MX_*_Init 调用的 HAL 函数检查通过。
 *
 * 在 main.c 的 USER CODE BEGIN 1 或 2 中加入:
 *   #ifdef SIMULATION
 *   SIM_Init();
 *   #endif
 * ═══════════════════════════════════════════════════════════════ */

static inline void SIM_Init(void)
{
    /* 使能所有外设时钟 */
    RCC->AHB1ENR |= 0xFFFFFFFF;   /* GPIO A-G, DMA1, DMA2, CRC */
    RCC->APB1ENR |= 0xFFFFFFFF;   /* TIM2-7, SPI2/3, USART2, CAN1/2 */
    RCC->APB2ENR |= 0xFFFFFFFF;   /* TIM1/8-11, USART1/6, ADC, SPI1 */

    printf("\r\n[SIM] SIMULATION MODE ACTIVE\r\n");
    printf("[SIM] HAL stubs: CAN SPI ETH\r\n");
}

#else /* !SIMULATION */

/* ================================================================
 * 真实硬件模式：所有宏为空
 * ================================================================ */

#define SIM_DELAY_US(us)
#define SIM_DELAY_MS(ms)
#define SIM_SKIP_LARGE_LOOP(count, label)
#define SIM_INJECT_VALUE(var, val)
#define SIM_Init()

#endif /* SIMULATION */

#endif /* __SIM_MODE_H */
