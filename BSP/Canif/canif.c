/*
 * canif.c — CAN 接口层实现（F407）
 *
 * 过滤器配置为"全接收"模式（MaskId=0），不过滤任何 ID。
 * 初始调试阶段不设硬件过滤，所有 ID 分组在软件层处理。
 */

#include "canif.h"
#include <stdio.h>

static CAN_TxHeaderTypeDef TxMessage;
static CAN_RxHeaderTypeDef RxMessage;

/* 全局传感器数据 */
volatile can_sensor_data_t g_can_sensor = {0};

/* ── 内部函数声明 ── */
static void CANFilter_Config(void);

/* ═══════════════════════ CAN 初始化 ═══════════════════════ */

uint8_t canif_init(void)
{
    /* 1. 配置过滤器（必须在 HAL_CAN_Start 之前） */
    CANFilter_Config();

    /* 2. 启动 CAN 外设 */
    printf("[CAN] attempting start...\r\n");
    HAL_StatusTypeDef can_st = HAL_CAN_Start(&hcan1);
    if (can_st != HAL_OK)
    {
        printf("[CAN] Start FAIL (code=%d)\r\n", (int)can_st);
        return 1;
    }

    printf("[CAN] Start OK (500kbps)\r\n");
    return 0;
}

/* ═══════════════════════ 过滤器配置 ═══════════════════════ */

static void CANFilter_Config(void)
{
    CAN_FilterTypeDef sFilterConfig;

    sFilterConfig.FilterBank = 0;                       /* 使用第 0 号过滤组        */
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;   /* 掩码模式                  */
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;  /* 32位: 单组可过滤1个ID     */
    sFilterConfig.FilterIdHigh = 0x0000;                /* 不过滤: ID=任意值          */
    sFilterConfig.FilterIdLow = 0x0000;
    sFilterConfig.FilterMaskIdHigh = 0x0000;            /* Mask=0: 任意ID都通过       */
    sFilterConfig.FilterMaskIdLow = 0x0000;
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;  /* 匹配后存入 FIFO0          */
    sFilterConfig.FilterActivation = CAN_FILTER_ENABLE; /* 使能该过滤组              */
    sFilterConfig.SlaveStartFilterBank = 14;            /* CAN1 使用 bank 0~13, 14为CAN2起始 */

    if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
    {
        printf("[CAN] Filter Config FAIL\r\n");
    }
}

/* ═══════════════════════ CAN 发送 ═══════════════════════ */

uint8_t CAN1_Send_Msg(uint32_t id, uint8_t *msg, uint8_t len)
{
    uint32_t txMailBox = CAN_TX_MAILBOX0;

    TxMessage.StdId = id;
    TxMessage.ExtId = id;
    TxMessage.IDE = CAN_ID_STD;
    TxMessage.RTR = CAN_RTR_DATA;
    TxMessage.DLC = len;

    if (HAL_CAN_AddTxMessage(&hcan1, &TxMessage, msg, &txMailBox) != HAL_OK)
        return 1;

    return 0;
}

/* ═══════════════════════ CAN 接收（轮询） ═══════════════════════ */

uint8_t CAN1_Recv_Msg(uint32_t *id, uint8_t *buf)
{
    if (HAL_CAN_GetRxFifoFillLevel(&hcan1, CAN_RX_FIFO0) == 0)
        return 0;

    HAL_CAN_GetRxMessage(&hcan1, CAN_RX_FIFO0, &RxMessage, buf);

    if (id != NULL)
        *id = RxMessage.StdId;

    return RxMessage.DLC;
}

/* ═══════════════════════ F103 控制数据解析 ═══════════════════════ */

void canif_parse_sensor(uint8_t *buf, uint8_t len)
{
    if (buf == NULL || len < 6) return;

    /*
     * 帧格式:
     * Byte 0: 温度整数+40 (-40~+110 → 0~150)
     * Byte 1: 温度小数×10 (0~9)
     * Byte 2: 湿度整数 (0~100)
     * Byte 3: 湿度小数×10 (0~9)
     * Byte 4: 旋钮值 0-255
     * Byte 5: 按键事件 (高4位=KEY_ID, 低4位=TYPE)
     */
    g_can_sensor.temperature  = ((int)buf[0] - 40) + (float)buf[1] * 0.1f;
    g_can_sensor.humidity     = (float)buf[2] + (float)buf[3] * 0.1f;
    g_can_sensor.knob         = buf[4];
    g_can_sensor.key_event    = buf[5];
    g_can_sensor.valid        = 1;
    g_can_sensor.tick         = HAL_GetTick();

    printf("[CAN] T=%.1f H=%.1f Knob=%u Key=0x%02X\r\n",
           (double)g_can_sensor.temperature,
           (double)g_can_sensor.humidity,
           g_can_sensor.knob, g_can_sensor.key_event);
}
