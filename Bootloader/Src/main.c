/*
 * main.c — F407 OTA Bootloader 入口
 *
 * Phase 1: 最小化跳转器
 *   初始化时钟 → GPIO(LED) → USART1(printf) → CAN1 → 跳转 APP
 *
 * Phase 2+: 检查 ota_flag → 进入 OTA 或跳转 APP
 *
 * 注意：本文件不使用 FreeRTOS/LVGL/SPI Flash/Touch
 *       所有外设在跳转前保持初始状态即可
 */

#include "main.h"
#include "can.h"
#include "usart.h"
#include "gpio.h"

#include "boot_manager.h"
#include "bootloader_main.h"
#include "inter_flash_cfg.h"
#include "iso_tp_cfg.h"
#include "en25q128.h"
#include "lfs_boot.h"
#include "sha256.h"
#include <stdio.h>

/* ── 全局变量 ── */
/* 用于 printf 的 HAL 句柄，定义在 usart.c */
extern UART_HandleTypeDef huart1;
extern CAN_HandleTypeDef hcan1;

/*
 * stm32f4xx_it.c 中 HAL_UART_RxCpltCallback 引用了这两个变量。
 * Bootloader 不使用 UART 中断接收，但为了链接通过而定义。
 */
volatile uint8_t  g_rx_byte = 0;
volatile uint8_t  g_rx_flag = 0;

/* ── 函数前置声明 ── */
static void SystemClock_Config(void);
static void PrintBootInfo(void);

/* ═══════════════════════════════════════════════════════════
 *  入口
 * ═══════════════════════════════════════════════════════════ */
int main(void)
{
    /* MCU HAL 初始化 */
    HAL_Init();

    /* 系统时钟: HSE 8MHz → PLL → 168MHz */
    SystemClock_Config();

    /* 外设初始化（最小集） */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_CAN1_Init();

    /* ── 启动信息 ── */
    PrintBootInfo();

    /* ── 初始化参数扇区 ── */
    inter_flash_cfg_init();

    /* ── 检查 OTA 更新标志 ── */
    int8_t ota_flag = inter_flash_cfg_get_app_update_flag();
    if (ota_flag == 1) {
        printf("[BOOT] OTA flag detected! Waiting for ISO-TP via CAN...\r\n");
        printf("[BOOT] CAN ID: DATA=0x%03X FC=0x%03X\r\n",
               CAN_ID_OTA_DATA, CAN_ID_OTA_FC);

        /* LED 常亮：OTA 模式指示 */
        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);

        /* 初始化 CAN + ISO-TP */
        iso_tp_init();

        /* 使能 SPI1 时钟并初始化 SPI Flash（用于 OTA 失败恢复） */
        __HAL_RCC_SPI1_CLK_ENABLE();
        EN25Q128_Init();
        EN25Q128_ReadID();  /* 验证 SPI Flash 通信 */

        uint8_t ota_ok = 0;

        /* 主循环：ISO-TP 接收 + Flash 写入 + LED 进度指示 */
        printf("[BOOT] Waiting for CAN data chunks...\r\n");
        uint32_t last_chunks = 0;
        while (1) {
            uint8_t st = iso_tp_poll();
            if (st == 2) {       /* OTA 完成 */
                ota_ok = 1;
                break;
            } else if (st == 1) { /* 收到数据块 */
                if (g_ota_chunks != last_chunks) {
                    printf("[BOOT] Chunks: %lu\r\n", g_ota_chunks);
                    last_chunks = g_ota_chunks;
                }
            }
            /* LED 闪烁：块数越多闪烁越快 */
            uint32_t period = 500;
            if (g_ota_chunks > 100) period = 30;
            else if (g_ota_chunks > 50)  period = 60;
            else if (g_ota_chunks > 20)  period = 120;
            else if (g_ota_chunks > 10)  period = 250;
            HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin,
                (HAL_GetTick() % (period * 2) < period) ?
                    GPIO_PIN_RESET : GPIO_PIN_SET);
            /* 简单超时：60 秒无数据则退出 */
            if (HAL_GetTick() > 60000) {
                printf("[BOOT] ISO-TP timeout\r\n");
                break;
            }
        }

        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);

        if (ota_ok) {
            printf("[BOOT] OTA via CAN success!\r\n");
            inter_flash_cfg_inc_ota_count();
            inter_flash_cfg_set_app_update_flag(0);
        } else {
            printf("[BOOT] OTA failed. Trying restore...\r\n");
            /* 先尝试 LFS 文件恢复 */
            int restored = 0;
            if (lfs_boot_init() == 0) {
                restored = (EN25Q128_RestoreFirmwareLFS() == 0);
                lfs_unmount(&g_lfs);
            }
            /* LFS 失败则降级到裸备份 */
            if (!restored) {
                printf("[BOOT] LFS restore failed, trying raw backup...\r\n");
                restored = (EN25Q128_RestoreFirmware() == 0);
            }
            if (restored) {
                printf("[BOOT] Restore OK, jumping to APP\r\n");
            } else {
                printf("[BOOT] No valid backup, clearing flag\r\n");
                inter_flash_cfg_set_app_update_flag(0);
            }
        }
    } else if (ota_flag == 0) {
        printf("[BOOT] No OTA pending. Jumping to APP...\r\n");
    } else {
        printf("[BOOT] ota_flag invalid (ret=%d). Jumping to APP anyway.\r\n", ota_flag);
    }

    /* ── 固件签名验证 ── */
    {
        int sig_ret = verify_firmware_sig();
        if (sig_ret == 0) {
            printf("[BOOT] Signature OK\r\n");
        } else {
            printf("[BOOT] Signature INVALID (ret=%d)!\r\n", sig_ret);
            printf("[BOOT] Trying SPI Flash restore...\r\n");
            int restored = 0;
            if (lfs_boot_init() == 0) {
                restored = (EN25Q128_RestoreFirmwareLFS() == 0);
                lfs_unmount(&g_lfs);
            }
            if (!restored)
                restored = (EN25Q128_RestoreFirmware() == 0);
            if (!restored) {
                printf("[BOOT] Restore FAILED, halting!\r\n");
                while (1) {
                    HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
                    for (volatile int d = 0; d < 1000000; d++);
                }
            }
            printf("[BOOT] Restore OK, jumping to APP\r\n");
        }
    }

    /* ── 跳转到 APP ── */
    printf("[BOOT] ---> jump to APP @ 0x%08lX\r\n\r\n", (uint32_t)APP_START_ADDR);
    boot_check_stack2jump_app(APP_START_ADDR);

    /* ── 不应到达此处 ── */
    Error_Handler();
}

/* ═══════════════════════════════════════════════════════════
 *  SysTick Handler — 驱动 HAL_GetTick() 供 ISO-TP 超时使用
 * ═══════════════════════════════════════════════════════════ */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ═══════════════════════════════════════════════════════════
 *  Error Handler
 * ═══════════════════════════════════════════════════════════ */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

/* ═══════════════════════════════════════════════════════════
 *  时钟配置 — 与 APP 完全一致
 *  168MHz: HSE 8MHz × PLLN=336 / PLLM=8 / PLLP=2
 * ═══════════════════════════════════════════════════════════ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;   /* APB1 = 42MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;   /* APB2 = 84MHz */
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

/* ═══════════════════════════════════════════════════════════
 *  启动信息打印
 * ═══════════════════════════════════════════════════════════ */
static void PrintBootInfo(void)
{
    uint32_t sysclk = HAL_RCC_GetSysClockFreq();
    uint32_t hclk   = HAL_RCC_GetHCLKFreq();
    uint32_t pclk1  = HAL_RCC_GetPCLK1Freq();
    uint32_t pclk2  = HAL_RCC_GetPCLK2Freq();

    printf("\r\n");
    printf("============================================\r\n");
    printf("  MY_OTA_GUI Bootloader v%d.%d.%d\r\n",
           BOOT_VERSION_MAJOR, BOOT_VERSION_MINOR, BOOT_VERSION_PATCH);
    printf("============================================\r\n");
    printf("  MCU     : STM32F407ZGT6 @ 168MHz\r\n");
    printf("  SYSCLK  : %lu MHz\r\n", sysclk / 1000000);
    printf("  APB1    : %lu MHz  (CAN: 500kbps)\r\n", pclk1 / 1000000);
    printf("  APB2    : %lu MHz  (SPI1: 42MHz)\r\n", pclk2 / 1000000);
    printf("  APP     : 0x%08lX\r\n", (uint32_t)APP_START_ADDR);
    printf("============================================\r\n");
}
