/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_fs_spi_flash.h"
#include "en25q128.h"
#include "touch/touch.h"
#include "canif.h"
#include "inter_flash_cfg.h"
#include "app_ui.h"
#include "mode_ui.h"
#include "settings_ui.h"
#include "clock_ui.h"
#include "spi_img_loader.h"
#include "lfs_port.h"
/* hw_diag.h — 探索期遗留，擦写 SPI Flash 影响启动时间，移除 */
/* #include "hw_diag.h" */

/* ── 自定义中文字体（OTA 进度提示用）── */
LV_FONT_DECLARE(ui_font_chinese_16);
/* USER CODE END Includes */

/* USER CODE BEGIN PFP */
/* 备份进度回调：仅刷屏不跑定时器（防 sysmon_timer 访问已销毁控件） */
static void backup_progress_cb(uint32_t cur, uint32_t tot, const char *phase)
{
    char buf[56];
    snprintf(buf, sizeof(buf), "OTA 备份中\n\n%s\n%lu / %lu", phase, (unsigned long)cur, (unsigned long)tot);
    lv_obj_t *child = lv_obj_get_child(lv_scr_act(), -1);
    if (child) lv_label_set_text(child, buf);
    lv_refr_now(NULL);  /* 只刷屏，不跑定时器 */
}
/* USER CODE END PFP */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern volatile uint8_t  g_rx_byte;
extern volatile uint8_t  g_rx_flag;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* USER CODE BEGIN Variables */
osThreadId_t guiTaskHandle;
const osThreadAttr_t guiTask_attributes = {
  .name = "guiTask",
  .stack_size = 1024 * 8,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

osThreadId_t touchTaskHandle;
const osThreadAttr_t touchTask_attributes = {
  .name = "touchTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

osThreadId_t canRxTaskHandle;
const osThreadAttr_t canRxTask_attributes = {
  .name = "canRxTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartGUITask(void *argument);
void StartTouchTask(void *argument);
void StartCanRxTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  guiTaskHandle  = osThreadNew(StartGUITask, NULL, &guiTask_attributes);
  touchTaskHandle = osThreadNew(StartTouchTask, NULL, &touchTask_attributes);
  canRxTaskHandle = osThreadNew(StartCanRxTask, NULL, &canRxTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  uint32_t last_tick = osKernelGetTickCount();
  uint8_t  ota_seq = 0;    /* OTA 触发序列检测状态机 */

  for(;;) {
    osDelay(10);

    /* LED 心跳 */
    if (osKernelGetTickCount() - last_tick >= 500) {
        last_tick = osKernelGetTickCount();
        HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
    }

    /* UART 回显 + OTA 触发检测 ("OTA" 或 "ota") */
    if (g_rx_flag) {
        g_rx_flag = 0;
        char c = (char)g_rx_byte;

        if (c == 'O' || c == 'o') ota_seq = 1;
        else if (ota_seq == 1 && (c == 'T' || c == 't')) ota_seq = 2;
        else if (ota_seq == 2 && (c == 'A' || c == 'a')) {
            ota_seq = 0;
            printf("\r\n[APP] OTA trigger via UART!\r\n");
            inter_flash_cfg_set_app_update_flag(1);
            HAL_Delay(100);
            NVIC_SystemReset();
        } else {
            ota_seq = 0;
        }

        HAL_UART_Transmit(&huart1, (uint8_t *)&g_rx_byte, 1, HAL_MAX_DELAY);
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_rx_byte, 1);
    }
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void StartGUITask(void *argument)
{
    printf("[GUI] Task start, free heap=%u\r\n", (unsigned)xPortGetFreeHeapSize());

    lv_init();
    lv_port_disp_init();
    printf("[GUI] LVGL+Display init done\r\n");

    /* ---- SPI Flash 驱动 + LVGL 文件系统 (图片按需从 Flash 读取) ---- */
    EN25Q128_Init();            /* SPI1 寄存器级重配 (Mode3, BR=SAFE)   */
    EN25Q128_SetSpeed(EN25Q128_BR_FAST);  /* 升速至 10.5MHz             */
    lv_fs_spi_flash_init();     /* 注册 "S:" 盘符 (只读)               */
    printf("[GUI] SPI Flash FS init done\r\n");

    /* ---- LittleFS 文件系统 (SPI Flash 0x000000~0xDFFFFF) ---- */
    lfs_storage_init();
    printf("[GUI] LittleFS init done\r\n");

    /* ---- CAN 接口启动 ---- */
    canif_init();

    /* 触摸屏初始化 + LVGL indev 注册 */
    touch_init();

    /* 从 SPI Flash 预加载图片到外部 SRAM */
    spi_img_load_all();

    /* 创建精简 GUI */
    app_ui_create();
    mode_ui_create();
    settings_ui_create();
    clock_ui_create();
    printf("[GUI] All UIs created (Home + Mode + Settings + Clock)\r\n");

    printf("[GUI] Init done, entering loop\r\n");

    for (;;) {
        lv_timer_handler();

        /* OTA 触发：由按钮回调设标志，主循环执行（确保 LVGL 刷新完毕） */
        if (g_ota_pending) {
            g_ota_pending = 0;
            printf("[GUI] OTA pending: showing screen, then backup\n");

            /* 显示备份提示画面 */
            lv_obj_clean(lv_scr_act());
            lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_t *lbl = lv_label_create(lv_scr_act());
            lv_label_set_text(lbl, "OTA 备份中\n\n正准备开始...");
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, &ui_font_chinese_16, LV_PART_MAIN);
            lv_obj_center(lbl);

            /* 多次调用 timer handler 确保 LCD 真正刷新 */
            for (int i = 0; i < 20; i++) {
                lv_timer_handler();
                osDelay(10);
            }

            /* 注册进度回调 → 备份期间实时刷新屏幕 */
            EN25Q128_SetBackupProgressCb(backup_progress_cb);
            EN25Q128_BackupFirmware();
            EN25Q128_SetBackupProgressCb(NULL);

            /* 更新提示 */
            lv_label_set_text(lbl, "OTA 备份完成!\n\n即将复位...");
            for (int i = 0; i < 10; i++) {
                lv_timer_handler();
                osDelay(10);
            }

            /* 设 OTA 标志并复位 */
            inter_flash_cfg_set_app_update_flag(1);
            osDelay(100);
            NVIC_SystemReset();
        }

        osDelay(5);
    }
}

/* ---- 独立触控轮询任务 (优先级高于 GUI, 不受渲染阻塞) ---- */
void StartTouchTask(void *argument)
{
    (void)argument;
    /* 等 GUI 任务完成触摸初始化 */
    osDelay(500);

    for (;;) {
        touch_poll();
        osDelay(10);  /* 100Hz 触控采样率 */
    }
}

/* ---- CAN 接收任务 (轮询 FIFO, 50Hz) ---- */
/* OTA 触发 CAN 帧 ID + Magic（与 uint3code 例程兼容） */
#define CAN_ID_CALL_OTA     0x0B3

void StartCanRxTask(void *argument)
{
    (void)argument;
    uint8_t rx_buf[8];
    uint32_t rx_id;
    uint32_t diag_tick = 0;

    /* 等 GUI 任务完成初始化 */
    osDelay(1000);

    printf("[CAN] canRxTask started\r\n");

    for (;;) {
        uint8_t len = CAN1_Recv_Msg(&rx_id, rx_buf);
        if (len > 0) {
            /* 识别 F103 传感器帧 (ID=0x12) */
            if (rx_id == CAN_ID_SENSOR) {
                canif_parse_sensor(rx_buf, len);
                /* 更新 GUI */
                uint8_t key_id   = (g_can_sensor.key_event >> 4) & 0x0F;
                uint8_t key_type = g_can_sensor.key_event & 0x0F;
                app_ui_update_sensor(g_can_sensor.temperature,
                                     g_can_sensor.humidity,
                                     g_can_sensor.knob,
                                     key_id, key_type);
                app_ui_set_can_status(1);
            }
            /* OTA 触发命令 (ID=0x0B3, Magic=BE AD BE EF) */
            else if (rx_id == CAN_ID_CALL_OTA && len >= 5) {
                if (rx_buf[0] == 0xBE && rx_buf[1] == 0xAD &&
                    rx_buf[2] == 0xBE && rx_buf[3] == 0xEF &&
                    rx_buf[4] == 0x01) {
                    printf("\r\n[CAN] OTA trigger via CAN!\r\n");
                    inter_flash_cfg_set_app_update_flag(1);
                    HAL_Delay(100);
                    NVIC_SystemReset();
                }
            }
            /* RTC 时间帧 (ID=0x13, F103 每秒发送) */
            else if (rx_id == 0x13 && len >= 6) {
                app_ui_update_time(rx_buf[0], rx_buf[1], rx_buf[2],
                                   rx_buf[3], rx_buf[4], rx_buf[5]);
            }
            /* 未知 ID — 仅调试时打印 */
            else {
                printf("[CAN] Unknown ID=0x%03lx L=%u\r\n",
                       (unsigned long)rx_id, len);
            }
        }
        osDelay(20);  /* 50Hz 轮询 */
    }
}
/* USER CODE END Application */

