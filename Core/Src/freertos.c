/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  */
/* USER CODE END Header */

#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* USER CODE BEGIN Includes */
#include "usart.h"
#include <stdio.h>
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_fs_spi_flash.h"
#include "gui_guider.h"
#include "events_init.h"
#include "en25q128.h"
#include "spiflash_images.h"
#include "spiflash_prog.h"
#include "touch/touch.h"
#include "spiflash_offset_table.h"
#include "canif.h"
/* USER CODE END Includes */

/* USER CODE BEGIN Variables */
extern volatile uint8_t  g_rx_byte;
extern volatile uint8_t  g_rx_flag;
/* USER CODE END Variables */

osThreadId_t guiTaskHandle;
const osThreadAttr_t guiTask_attributes = {
  .name = "guiTask",
  .stack_size = 1024 * 8,  /* LVGL v9.3 渲染栈消耗大, 4KB 不够 */
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t touchTaskHandle;
const osThreadAttr_t touchTask_attributes = {
  .name = "touchTask",
  .stack_size = 128 * 4,                        /* 仅 touch_poll + osDelay, 512B 足够 */
  .priority = (osPriority_t) osPriorityAboveNormal, /* 高于 GUI 任务, 确保触摸轮询不被渲染阻塞 */
};

osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t canRxTaskHandle;
const osThreadAttr_t canRxTask_attributes = {
  .name = "canRxTask",
  .stack_size = 512 * 4,                         /* 2048B: printf+%f 浮点格式化需要大栈 */
  .priority = (osPriority_t) osPriorityNormal,
};

lv_ui guider_ui;

/* USER CODE BEGIN FunctionPrototypes */
void StartGUITask(void *argument);
void StartTouchTask(void *argument);
void StartCanRxTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void MX_FREERTOS_Init(void);

void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  /* USER CODE BEGIN RTOS_THREADS */
  guiTaskHandle  = osThreadNew(StartGUITask, NULL, &guiTask_attributes);
  touchTaskHandle = osThreadNew(StartTouchTask, NULL, &touchTask_attributes);
  canRxTaskHandle = osThreadNew(StartCanRxTask, NULL, &canRxTask_attributes);
  /* USER CODE END RTOS_THREADS */
}

void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  uint32_t last_tick = osKernelGetTickCount();
  for(;;) {
    osDelay(10);
    if (osKernelGetTickCount() - last_tick >= 500) {
        last_tick = osKernelGetTickCount();
        HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
    }
    if (g_rx_flag) {
        g_rx_flag = 0;
        HAL_UART_Transmit(&huart1, (uint8_t *)&g_rx_byte, 1, HAL_MAX_DELAY);
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_rx_byte, 1);
    }
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Application */

/* ---- Mode 屏温度更新回调 (LVGL timer, GUI task 上下文) ---- */
static void mode_temp_update_cb(lv_timer_t *t)
{
    (void)t;

    /* 有 CAN 数据且 10 秒内有效 */
    if (!g_can_sensor.valid || (HAL_GetTick() - g_can_sensor.tick > 10000U))
        return;

    /* 保护: 温度标签和图标必须存在 */
    if (guider_ui.mode_label_temp_num == NULL || guider_ui.mode_img_temper == NULL)
        return;

    /* ---- 温度: 带一位小数 ---- */
    lv_label_set_text_fmt(guider_ui.mode_label_temp_num, "%.1f C",
                          (double)g_can_sensor.temperature);

    /* 高温 (＞60°C) 时温度图标变红提示 */
    if (g_can_sensor.temperature > 60.0f) {
        lv_obj_set_style_img_recolor_opa(guider_ui.mode_img_temper, 255,
                                         LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(guider_ui.mode_img_temper,
                                     lv_color_hex(0xf00000),
                                     LV_PART_MAIN|LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_img_recolor_opa(guider_ui.mode_img_temper, 0,
                                         LV_PART_MAIN|LV_STATE_DEFAULT);
    }

    /* ---- 湿度标签 (首次运行时若不存在则创建) ---- */
    if (guider_ui.mode_label_tempter == NULL) {
        guider_ui.mode_label_tempter = lv_label_create(guider_ui.mode_tileview_mode_f);
        lv_obj_set_pos(guider_ui.mode_label_tempter, 250, 180);
        lv_obj_set_size(guider_ui.mode_label_tempter, 200, 60);
        lv_obj_set_style_text_font(guider_ui.mode_label_tempter,
                                   &lv_font_montserrat_28, LV_PART_MAIN|LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(guider_ui.mode_label_tempter,
                                    lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
    }
    lv_label_set_text_fmt(guider_ui.mode_label_tempter, "H: %.1f%%",
                          (double)g_can_sensor.humidity);
}

/* ================================================================
 * 硬件诊断: 外部 SRAM 16-bit + SPI Flash 读写
 * ================================================================ */
static void hw_diag(void)
{
    int pass = 0, fail = 0;

    /* ===== 1. 外部 SRAM 16-bit 读写 ===== */
    printf("\r\n=== SRAM 16-bit Diag @ 0x68000000 ===\r\n");
    volatile uint16_t *sram = (volatile uint16_t *)0x68000000;
    uint16_t orig0 = sram[0];
    uint16_t orig1 = sram[1000];

    /* 0xAA55 / 0x55AA — 每个 bit 都翻转 */
    uint16_t pat;
    pat = 0xAA55; sram[0] = pat;
    printf("  wr=0x%04X rd=0x%04X %s\r\n", pat, sram[0], sram[0] == pat ? "[PASS]" : "[FAIL]");
    if (sram[0] == pat) pass++; else fail++;

    pat = 0x55AA; sram[0] = pat;
    printf("  wr=0x%04X rd=0x%04X %s\r\n", pat, sram[0], sram[0] == pat ? "[PASS]" : "[FAIL]");
    if (sram[0] == pat) pass++; else fail++;

    /* Walking-1: 16 个单 bit 逐一走一遍 */
    uint16_t w1 = 1;
    int wp = 0;
    for (int i = 0; i < 16; i++) {
        sram[0] = w1;
        if (sram[0] == w1) wp++;
        w1 <<= 1;
    }
    printf("  Walking-1: %d/16 %s\r\n", wp, wp == 16 ? "[PASS]" : "[FAIL]");
    if (wp == 16) pass++; else fail++;

    /* Walking-0: 16 个单 bit 清零 */
    uint16_t w0 = 0xFFFE;
    wp = 0;
    for (int i = 0; i < 16; i++) {
        sram[0] = w0;
        if (sram[0] == w0) wp++;
        w0 = (w0 << 1) | 1;
    }
    printf("  Walking-0: %d/16 %s\r\n", wp, wp == 16 ? "[PASS]" : "[FAIL]");
    if (wp == 16) pass++; else fail++;

    /* 另一地址验证不是总线电容残留 */
    pat = 0x1234; sram[1000] = pat;
    printf("  addr+2000: wr=0x%04X rd=0x%04X %s\r\n", pat, sram[1000], sram[1000] == pat ? "[PASS]" : "[FAIL]");
    if (sram[1000] == pat) pass++; else fail++;
    sram[1000] = orig1;
    sram[0]    = orig0;

    printf("  SRAM: %d pass, %d fail\r\n", pass, fail);

    /* ===== 2. SPI Flash ===== */
    printf("\r\n=== SPI Flash Diag ===\r\n");

    /* JEDEC ID */
    uint32_t id = EN25Q128_ReadID();
    printf("  JEDEC ID: 0x%06lX (%s)\r\n", id,
           id == EN25Q128_EXPECTED_ID ? "OK" : "MISMATCH");

    /* Status */
    uint8_t sr1 = EN25Q128_ReadSR1();
    uint8_t sr2 = EN25Q128_ReadSR2();
    printf("  SR1=0x%02X (BUSY=%d) SR2=0x%02X\r\n", sr1, (sr1 & 0x01) ? 1 : 0, sr2);

    /* ---- 诊断: 遍历全部图片检查 LVGL header (magic=0x19) ---- */
    printf("\r\n  SPI Flash header check (all images):\r\n");
    
    #define CHK_ENTRY(name) { OFFSET_##name, #name }
    static const struct { uint32_t off; const char *label; } img_tbl[] = {
        CHK_ENTRY(_BATTERY_BAK_RGB565A8_43X24_MAP),
        CHK_ENTRY(_BATTERY_IND_RGB565A8_43X24_MAP),
        CHK_ENTRY(_DET_BAK_RGB565A8_800X480_MAP),
        CHK_ENTRY(_DIRECTION_RGB565A8_50X42_MAP),
        CHK_ENTRY(_HIGH_BEAM_RGB565A8_46X46_MAP),
        CHK_ENTRY(_HOME_RGB565A8_43X43_MAP),
        CHK_ENTRY(_IMAGE_GREEN_RGB565A8_350X350_MAP),
        CHK_ENTRY(_IMAGE_RED_RGB565A8_350X350_MAP),
        CHK_ENTRY(_IMG_NAV_1_RGB565A8_116X116_MAP),
        CHK_ENTRY(_KMBG_RGB565A8_166X166_MAP),
        CHK_ENTRY(_LIGHT_RGB565A8_46X46_MAP),
        CHK_ENTRY(_LOGO_RGB565A8_100X35_MAP),
        CHK_ENTRY(_MONITOR_RGB565A8_41X37_MAP),
        CHK_ENTRY(_MOTO_RGB565A8_800X480_MAP),
        CHK_ENTRY(_MUSIC_COVER_1_RGB565A8_183X183_MAP),
        CHK_ENTRY(_MUSIC_COVER_2_RGB565A8_183X183_MAP),
        CHK_ENTRY(_MUSIC_COVER_3_RGB565A8_183X183_MAP),
        CHK_ENTRY(_SUNNY_RGB565A8_66X66_MAP),
        CHK_ENTRY(_TEM_RGB565A8_91X123_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_0_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_01_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_02_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_03_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_04_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_05_MAP),
        CHK_ENTRY(MODE_ANIMIMG_MAPMAP_06_MAP),
    };
    #undef CHK_ENTRY
    
    int hdr_ok = 0, hdr_bad = 0, hdr_empty = 0;
    uint8_t hdr[12];
    for (int i = 0; i < (int)(sizeof(img_tbl)/sizeof(img_tbl[0])); i++) {
        EN25Q128_Read(hdr, img_tbl[i].off, 12);
        if (hdr[0] == 0x19) {
            hdr_ok++;
        } else if (hdr[0] == 0xFF) {
            hdr_empty++;
            printf("  [ERASED] @ 0x%08lX %s\r\n", img_tbl[i].off, img_tbl[i].label);
        } else {
            hdr_bad++;
            printf("  [BAD 0x%02X] @ 0x%08lX %s\r\n", hdr[0], img_tbl[i].off, img_tbl[i].label);
        }
    }
    printf("  Header result: %d OK, %d EMPTY, %d BAD (total %d)\r\n",
           hdr_ok, hdr_empty, hdr_bad, hdr_ok + hdr_empty + hdr_bad);

    /* ---- moto 背景深度验证 (头部 + 像素首尾 + 随机采样) ---- */
    printf("\r\n  moto pixel verify (512B head + 512B mid + 512B tail):\r\n");
    EN25Q128_Read(hdr, OFFSET__MOTO_RGB565A8_800X480_MAP, 12);
    if (hdr[0] != 0x19) {
        printf("  SKIP — moto header missing\r\n");
    } else {
        uint16_t mw = hdr[4] | ((uint16_t)hdr[5] << 8);
        uint16_t mh = hdr[6] | ((uint16_t)hdr[7] << 8);
        uint16_t ms = hdr[8] | ((uint16_t)hdr[9] << 8);
        printf("  Header: w=%u h=%u stride=%u\r\n", mw, mh, ms);

        uint32_t base  = OFFSET__MOTO_RGB565A8_800X480_MAP + 12;  /* skip header */
        uint32_t total = (uint32_t)mw * mh * 2;  /* RGB565 color plane bytes */
        uint32_t mid   = total / 2;
        uint32_t tail  = total - 512;

        #define VFY_SZ 512
        uint8_t spi_v[VFY_SZ];
        int vfy_pass = 0, vfy_total = 0;
        
        /* moto 已走 S: 路径, 无 SRAM 副本可对比 */
        printf("  SKIP — SRAM moto data not loaded\r\n");
        #undef VFY_SZ
    }

    /* Erase+Write+Read 回路测试 (最后扇区 0x00FFF000) */
    uint32_t test_sec = 0x00FFF000;
    uint8_t wbuf[256], rbuf[256];
    for (int i = 0; i < 256; i++) wbuf[i] = (uint8_t)i;

    printf("  EraseWrite+Read @ 0x%08lX ... ", test_sec);
    EN25Q128_EraseWrite(wbuf, test_sec, 256);
    EN25Q128_Read(rbuf, test_sec, 256);

    int r_ok = 1;
    for (int i = 0; i < 256; i++) { if (wbuf[i] != rbuf[i]) { r_ok = 0; break; } }
    printf("%s\r\n", r_ok ? "PASS" : "FAIL (first mismatch below)");
    if (!r_ok) {
        for (int i = 0; i < 256; i++) {
            if (wbuf[i] != rbuf[i]) {
                printf("  @[%d] wr=0x%02X rd=0x%02X\r\n", i, wbuf[i], rbuf[i]);
                break;
            }
        }
    }
    printf("=== HW Diag Done ===\r\n\r\n");
}

void StartGUITask(void *argument)
{
    printf("[GUI] Task start, free heap=%u\r\n", (unsigned)xPortGetFreeHeapSize());

    lv_init();
    lv_tick_set_cb((lv_tick_get_cb_t)HAL_GetTick);  /* 复用 HAL 1ms tick (TIM3) */
    lv_port_disp_init();
    printf("[GUI] LVGL+Display init done\r\n");

    /* ---- SPI Flash 驱动 + LVGL 文件系统 (图片按需从 Flash 读取) ---- */
    EN25Q128_Init();            /* SPI1 寄存器级重配 (Mode3, BR=SAFE)   */
    EN25Q128_SetSpeed(EN25Q128_BR_FAST);  /* 升速至 10.5MHz             */
    lv_fs_spi_flash_init();     /* 注册 "S:" 盘符 (只读)               */
    printf("[GUI] SPI Flash FS init done\r\n");

    /* ---- 硬件诊断 (SRAM 16bit + SPI Flash) ---- */
    hw_diag();

    /* ---- CAN 接口启动 ---- */
    canif_init();

    /* ---- 预加载小图片到外部 SRAM (16-bit 诊断 5/5 PASS) ---- */
    spiflash_images_init();

    /* 触摸屏初始化 + LVGL indev 注册 */
    touch_init();

    setup_ui(&guider_ui);
    events_init(&guider_ui);
    printf("[GUI] UI setup done, entering loop\r\n");

    /* ---- Mode 屏温度更新定时器 (500ms, 有 CAN 数据时刷新) ---- */
    lv_timer_create(mode_temp_update_cb, 500, &guider_ui);

    for (;;) {
        lv_timer_handler();
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

    /* 等 GUI 任务完成初始化 */
    osDelay(1000);

    printf("[CAN] canRxTask started\r\n");

    for (;;) {
        uint8_t len = CAN1_Recv_Msg(&rx_id, rx_buf);
        if (len > 0) {
            /* 识别 F103 传感器帧 (ID=0x12) */
            if (rx_id == CAN_ID_SENSOR) {
                canif_parse_sensor(rx_buf, len);
            } else {
                printf("[CAN] Unknown ID=0x%03lx L=%u\r\n",
                       (unsigned long)rx_id, len);
            }
        }
        osDelay(20);  /* 50Hz 轮询 */
    }
}
/* USER CODE END Application */
