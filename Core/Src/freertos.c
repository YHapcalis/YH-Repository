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
#include "camera_ui.h"
#include "ov7670.h"
#include "spi_img_loader.h"
#include "lfs_port.h"
#include "wifi_function.h"          /* ESP8266 WiFi 模块 AT 驱动 */
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

/* -- FreeRTOS 任务栈大小定义 (所有尺寸以字为单位) -- */
#define GUI_TASK_STACK_SIZE     (1024 * 24)  /* 24KB: LVGL + camera, 翻倍预留 (原 12KB, 高水位 ~9.5KB) */
#define TOUCH_TASK_STACK_SIZE   (1024 * 4)   /*  4KB: 触控 I2C 轮询, 重点扩充 (原 512B, 高水位 425B) */
#define CANRX_TASK_STACK_SIZE   (1024 * 4)   /*  4KB: CAN 接收 + printf, 翻倍 (原 2KB) */
#define WIFI_TASK_STACK_SIZE    (1024 * 4)   /*  4KB: ESP8266 AT + sprintf, 翻倍 (原 2KB) */
#define DEFAULT_TASK_STACK_SIZE (1024 * 4)   /*  4KB: LED + UART 回显, 重点扩充 (原 512B, 高水位 413B) */

osThreadId_t guiTaskHandle;
const osThreadAttr_t guiTask_attributes = {
  .name = "guiTask",
  .stack_size = GUI_TASK_STACK_SIZE,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

osThreadId_t touchTaskHandle;
const osThreadAttr_t touchTask_attributes = {
  .name = "touchTask",
  .stack_size = TOUCH_TASK_STACK_SIZE,
  .priority = (osPriority_t) osPriorityAboveNormal,
};

osThreadId_t canRxTaskHandle;
const osThreadAttr_t canRxTask_attributes = {
  .name = "canRxTask",
  .stack_size = CANRX_TASK_STACK_SIZE,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t wifiTaskHandle;
const osThreadAttr_t wifiTask_attributes = {
  .name = "wifiTask",
  .stack_size = WIFI_TASK_STACK_SIZE,
  .priority = (osPriority_t) osPriorityBelowNormal,  /* 低优先级, 不干扰主界面响应 */
};
/* ── CAN → LVGL 延迟更新标志 (CAN 任务只设标志, GUI 任务消费, 避免线程冲突) ── */
volatile uint8_t g_can_sensor_pending = 0;
volatile uint8_t g_can_time_pending   = 0;
volatile uint8_t g_can_time_buf[6]    = {0};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = DEFAULT_TASK_STACK_SIZE,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/**
 * ============================================================
 *  GUI 主任务 (APPLICATION 层)
 * ============================================================
 * 职责: 管理整个图形界面生命周期
 *
 * 初始化顺序 (必须):
 *   1. LVGL 内核 + 显示驱动 (lcd_init / lv_port_disp_init)
 *   2. SPI Flash 驱动 (图片文件系统基石)
 *   3. LittleFS 挂载 (管理文件备份)
 *   4. CAN 接口启动 (接收 F103 传感器数据)
 *   5. 触摸屏初始化 + 注册 LVGL 输入设备
 *   6. 从 SPI Flash 预加载图片到外部 SRAM
 *   7. 创建四屏 UI 对象 (Home / Mode / Settings / Clock)
 *
 * 主循环逻辑:
 *   - 摄像头激活时: camera_refresh + lv_timer_handler
 *   - 正常模式: lv_timer_handler(处理触摸事件+渲染)
 *   - 每次循环检查 CAN 延迟更新标志
 *   - osDelay(5) 让出 CPU 给低优先级任务
 */

void StartGUITask(void *argument);
void StartTouchTask(void *argument);
void StartCanRxTask(void *argument);
void StartWiFiTask(void *argument);
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
  wifiTaskHandle  = osThreadNew(StartWiFiTask, NULL, &wifiTask_attributes);
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
        /* ── 处理 CAN 数据延迟更新 (避免 CAN 任务直接调用 LVGL) ── */
        if (g_can_sensor_pending) {
            g_can_sensor_pending = 0;
            uint8_t key_id   = (g_can_sensor.key_event >> 4) & 0x0F;
            uint8_t key_type = g_can_sensor.key_event & 0x0F;
            app_ui_update_sensor(g_can_sensor.temperature,
                                 g_can_sensor.humidity,
                                 g_can_sensor.knob,
                                 key_id, key_type);
            app_ui_set_can_status(1);
        }
        if (g_can_time_pending) {
            g_can_time_pending = 0;
            app_ui_update_time(g_can_time_buf[0], g_can_time_buf[1],
                               g_can_time_buf[2], g_can_time_buf[3],
                               g_can_time_buf[4], g_can_time_buf[5]);
        }

        if (g_camera_active) {
            camera_refresh();
            lv_timer_handler();  /* 保持 LVGL 活性（Back 按钮响应、屏幕刷新） */
            osDelay(5);
            continue;
        }
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
            /* LFS 文件备份（首选），失败降级到裸备份 */
            if (EN25Q128_BackupFirmwareLFS() != 0) {
                printf("[GUI] LFS backup failed, fallback to raw...\n");
                EN25Q128_BackupFirmware();
            }
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
                g_can_sensor_pending = 1;  /* GUI 任务统一更新 LVGL, 避免线程冲突 */
            }
            /* OTA 触发命令 (ID=0x0B3, Magic=BE AD BE EF) */
            else if (rx_id == CAN_ID_CALL_OTA && len >= 5) {
                if (rx_buf[0] == OTA_MAGIC_0 && rx_buf[1] == OTA_MAGIC_1 &&
                    rx_buf[2] == OTA_MAGIC_2 && rx_buf[3] == OTA_MAGIC_3 &&
                    rx_buf[4] == OTA_CMD_EXECUTE) {
                    printf("\r\n[CAN] OTA trigger via CAN!\r\n");
                    inter_flash_cfg_set_app_update_flag(1);
                    HAL_Delay(100);
                    NVIC_SystemReset();
                }
            }
            /* RTC 时间帧 (ID=0x13, F103 每秒发送) */
            else if (rx_id == CAN_ID_RTC_TIME && len >= 6) {
                for (int i = 0; i < 6; i++) g_can_time_buf[i] = rx_buf[i];
                g_can_time_pending = 1;
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

/* ---- WiFi 任务 (ESP8266 初始化 + 连接维护) ---- */
#define WIFI_SSID       "Lee"          /* ← 修改为你的 WiFi 名称 */
#define WIFI_PASSWORD   ""      /* ← 修改为你的 WiFi 密码 */

void StartWiFiTask(void *argument)
{
    (void)argument;
    printf("[WiFi] Task started, waiting for system ready...\r\n");
    osDelay(3000);  /* 等 GUI/LVGL/SPI-Flash 全部就绪 */

    /* 使能 ESP8266 电源 (CH_PD = HIGH) */
    ESP8266_Choose(ENABLE);
    osDelay(500);

    /* 使能 USART3 RX 中断 — ESP8266 数据通过中断填充 wifi_config 缓冲区 */
    USART3->CR1 |= USART_CR1_RXNEIE;
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    printf("[WiFi] USART3 RX interrupt enabled\r\n");

    /* 复位 ESP8266 并测试 AT */
    ESP8266_AT_Test();
    printf("[WiFi] AT OK -- module ready\r\n");

    /* 设置为 Station 模式 (连接外部路由器) */
    ESP8266_Net_Mode_Choose(STA);
    printf("[WiFi] Mode set to STA\r\n");

    /* 连接 WiFi 热点 */
    printf("[WiFi] Connecting to \"%s\"...\r\n", WIFI_SSID);
    if (ESP8266_JoinAP(WIFI_SSID, WIFI_PASSWORD)) {
        printf("[WiFi] Connected!\r\n");
        /* 查询并打印 IP */
        ESP8266_Cmd("AT+CIFSR", "OK", NULL, 1000);
        /* 启用多连接 */
        ESP8266_Enable_MultipleId(ENABLE);
        /* TCP Server (port 8080, for directed / sequential OTA) */
        if (ESP8266_StartOrShutServer(ENABLE, "8080", "0")) {
            printf("[WiFi] TCP server on port 8080 ready\r\n");
        }
    } else {
        printf("[WiFi] Failed to connect! Will retry...\r\n");
    }

    /* 主循环: 保持连接 + OTA 接收 */
    {
        uint32_t tick30 = 0;
        uint32_t ota_offset = 0;       /* 已接收字节数 */
        uint32_t ota_total  = 0;       /* 固件总大小 */
        uint8_t  ota_active = 0;       /* 正在 OTA 接收中 */
        uint8_t  ota_buf[256];         /* 临时写入缓冲区 */
        char     rx_line[1024];         /* 一行接收数据 */

        for (;;) {
            osDelay(10);
            tick30++;

            /* --- 非阻塞检查 ESP8266 接收帧 (不调用 ESP8266_ReceiveString, 它阻塞) --- */
            if (strEsp8266_Fram_Record.InfBit.FramFinishFlag) {
                strEsp8266_Fram_Record.InfBit.FramFinishFlag = 0;
                uint16_t flen = strEsp8266_Fram_Record.InfBit.FramLength;
                if (flen > sizeof(rx_line) - 1) flen = sizeof(rx_line) - 1;
                memcpy(rx_line, strEsp8266_Fram_Record.Data_RX_BUF, flen);
                rx_line[flen] = '\0';
                strEsp8266_Fram_Record.InfBit.FramLength = 0;

                /* +IPD frame: +IPD,<id>,<len>:<data> (MUX-mode with ID) */
                char *ipd = strstr(rx_line, "+IPD");
                if (ipd) {
                    /* extract data length (skip connection ID) */
                    uint32_t ipd_len = 0;
                    char *p = ipd + 5;
                    for (; *p && *p != ','; p++); /* skip ID */
                    if (*p == ',') {
                        p++;
                        for (; *p && *p != ':'; p++)
                            ipd_len = ipd_len * 10 + (uint32_t)(*p - '0');
                    }
                    /* point to data after colon */
                    char *p_data = strchr(ipd, ':');
                    if (p_data && ipd_len > 0 && ipd_len < 1024) {
                        p_data++;

                        /* WHO: reply with MCU UID (direct mode: AT+CIPSEND) */
                        if (strncmp(p_data, "WHO", 3) == 0) {
                            uint32_t uid0 = *(uint32_t*)0x1FFF7A10;
                            uint32_t uid1 = *(uint32_t*)0x1FFF7A14;
                            uint32_t uid2 = *(uint32_t*)0x1FFF7A18;
                            char rbuf[40];
                            int rn = snprintf(rbuf, sizeof(rbuf),
                                "ID:%08lX%08lX%08lX\r\n", uid0, uid1, uid2);
                            if (rn > 0) {
                                ESP8266_Usart("AT+CIPSEND=%d\r\n", rn);
                                osDelay(100);
                                ESP8266_Usart("%s", rbuf);
                            }
                            continue;
                        }

                        /* OTA:size (directed) or OTA:size:ip:port (broadcast) */
                        if (strncmp(p_data, "OTA:", 4) == 0 && !ota_active) {
                            char *sp = p_data + 4;
                            ota_total = 0;
                            for (; *sp && *sp != ':'; sp++)
                                ota_total = ota_total * 10 + (uint32_t)(*sp - '0');
                            if (ota_total > 4096 && ota_total <= 0x100000) {
                                char *srv_ip = 0, *srv_port = 0;
                                if (*sp == ':') {
                                    sp++; srv_ip = sp;
                                    for (; *sp && *sp != ':'; sp++);
                                    if (*sp == ':') { *sp = 0; srv_port = sp + 1; }
                                }
                                if (srv_ip && srv_port) {
                                    /* Broadcast: close UDP, connect TCP download */
                                    printf("[WiFi] OTA broadcast: %lu @ %s:%s\r\n",
                                           ota_total, srv_ip, srv_port);
                                    ESP8266_Usart("AT+CIPCLOSE\r\n"); osDelay(500);
                                    char cbuf[100];
                                    snprintf(cbuf, sizeof(cbuf),
                                        "AT+CIPSTART=\"TCP\",\"%s\",%s",srv_ip,srv_port);
                                    if (ESP8266_Cmd(cbuf, "OK", NULL, 3000)) {
                                        ota_active = 1; ota_offset = 8;
                                        for (uint32_t s = 0; s < 128; s++)
                                            EN25Q128_EraseSector(SPI_BAK_ADDR + s * 4096);
                                    }
                                } else {
                                    /* Directed: already TCP-connected */
                                    ota_active = 1; ota_offset = 8;
                                    for (uint32_t s = 0; s < 128; s++)
                                        EN25Q128_EraseSector(SPI_BAK_ADDR + s * 4096);
                                }
                            }
                            continue;
                        }
                        /* OTA data receive */
                        if (ota_active) {
                            uint32_t n = ipd_len;
                            if (n > 0 && n <= 256 && ota_offset - 8 + n <= ota_total) {
                                memcpy(ota_buf, p_data, n);
                                EN25Q128_Write(ota_buf, SPI_BAK_ADDR + ota_offset, n);
                                ota_offset += n;
                            }
                        }
                    }
                }
            }

            /* --- OTA 完成检查 --- */
            if (ota_active && ota_offset - 8 >= ota_total) {
                printf("[WiFi] OTA receive complete! (%lu bytes)\r\n", ota_total);
                uint8_t hdr[8];
                hdr[0] = (uint8_t)(SPI_BAK_MAGIC >> 0);
                hdr[1] = (uint8_t)(SPI_BAK_MAGIC >> 8);
                hdr[2] = (uint8_t)(SPI_BAK_MAGIC >> 16);
                hdr[3] = (uint8_t)(SPI_BAK_MAGIC >> 24);
                hdr[4] = (uint8_t)(ota_total >> 0);
                hdr[5] = (uint8_t)(ota_total >> 8);
                hdr[6] = (uint8_t)(ota_total >> 16);
                hdr[7] = (uint8_t)(ota_total >> 24);
                EN25Q128_Write(hdr, SPI_BAK_ADDR, 8);

                printf("[WiFi] Rebooting into bootloader for OTA...\r\n");
                osDelay(500);
                inter_flash_cfg_set_app_update_flag(1);
                osDelay(100);
                NVIC_SystemReset();
            }

            /* --- 每 30 秒保活检测 --- */
            if (tick30 >= 3000) {  /* 3000 × 10ms = 30s */
                tick30 = 0;
                if (!ESP8266_Cmd("AT", "OK", NULL, 200)) {
                    printf("[WiFi] Lost, reconnecting...\r\n");
                    ESP8266_Rst(); osDelay(1000);
                    ESP8266_AT_Test();
                    ESP8266_Net_Mode_Choose(STA);
                    ESP8266_JoinAP(WIFI_SSID, WIFI_PASSWORD);
                    ESP8266_Enable_MultipleId(ENABLE);
                    ESP8266_StartOrShutServer(ENABLE, "8080", "0");
                }
            }
        }
    }
}
/* USER CODE END Application */

