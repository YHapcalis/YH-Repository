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
#include "ymodem.h"
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
        printf("[BOOT] OTA flag detected! Waiting for Ymodem...\r\n");
        printf("[BOOT] Send .bin file via Ymodem (115200-8N1)\r\n");

        /* LED 常亮：视觉提示已进入 Bootloader OTA 模式 */
        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);

        /* 接收固件（阻塞直到完成或失败） */
        uint32_t file_size = 0;
        COM_StatusTypeDef ymodem_ret = Ymodem_Receive(&file_size);

        /* Ymodem 结束（不论成功失败），关闭 LED */
        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);

        if (ymodem_ret == COM_OK) {
            printf("[BOOT] OTA success! (%lu bytes)\r\n", file_size);
            inter_flash_cfg_set_app_update_flag(0);
        } else {
            printf("[BOOT] OTA failed (ret=%d). Clearing flag and booting...\r\n",
                   ymodem_ret);
            inter_flash_cfg_set_app_update_flag(0);
        }
    } else if (ota_flag == 0) {
        printf("[BOOT] No OTA pending. Jumping to APP...\r\n");
    } else {
        printf("[BOOT] ota_flag invalid (ret=%d). Jumping to APP anyway.\r\n", ota_flag);
    }

    /* ── 跳转到 APP ── */
    printf("[BOOT] ---> jump to APP @ 0x%08lX\r\n\r\n", (uint32_t)APP_START_ADDR);
    boot_check_stack2jump_app(APP_START_ADDR);

    /* ── 不应到达此处 ── */
    Error_Handler();
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
