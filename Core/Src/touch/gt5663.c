/**
 * @file    gt5663.c
 * @brief   GT5663/GT5688 电容触摸驱动
 *          改编自官方 36-触摸屏实验 gt5663.c, 精简版 (去校准/去PID打印/去配置表)
 */

#include "gt5663.h"
#include "ctiic.h"
#include <stdio.h>
#include <string.h>

/* ---- 配置表 (184 bytes) ---- */
static const uint8_t GT5663_CFG_TBL[] = {
    0X60,0XE0,0X01,0X20,0X03,0X05,0X35,0X00,0X02,0X08,
    0X1E,0X08,0X50,0X3C,0X0F,0X05,0X00,0X00,0XFF,0X67,
    0X50,0X00,0X00,0X18,0X1A,0X1E,0X14,0X89,0X28,0X0A,
    0X30,0X2E,0XBB,0X0A,0X03,0X00,0X00,0X02,0X33,0X1D,
    0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X32,0X00,0X00,
    0X2A,0X1C,0X5A,0X94,0XC5,0X02,0X07,0X00,0X00,0X00,
    0XB5,0X1F,0X00,0X90,0X28,0X00,0X77,0X32,0X00,0X62,
    0X3F,0X00,0X52,0X50,0X00,0X52,0X00,0X00,0X00,0X00,
    0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
    0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X0F,
    0X0F,0X03,0X06,0X10,0X42,0XF8,0X0F,0X14,0X00,0X00,
    0X00,0X00,0X1A,0X18,0X16,0X14,0X12,0X10,0X0E,0X0C,
    0X0A,0X08,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
    0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
    0X00,0X00,0X29,0X28,0X24,0X22,0X20,0X1F,0X1E,0X1D,
    0X0E,0X0C,0X0A,0X08,0X06,0X05,0X04,0X02,0X00,0XFF,
    0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00,
    0X00,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,0XFF,
    0XFF,0XFF,0XFF,0XFF,
};

touch_data_t g_touch;

/* 5 点数据寄存器 */
static const uint16_t TPX_TBL[5] = {GT_TP1_REG,GT_TP2_REG,GT_TP3_REG,GT_TP4_REG,GT_TP5_REG};

/* ---- 写寄存器 (16-bit 地址) ---- */
uint8_t GT5663_WR_Reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t ret = 0;
    CT_IIC_Start();
    CT_IIC_Send_Byte(GT_CMD_WR);
    CT_IIC_Wait_Ack();
    CT_IIC_Send_Byte(reg >> 8);
    CT_IIC_Wait_Ack();
    CT_IIC_Send_Byte(reg & 0xFF);
    CT_IIC_Wait_Ack();
    for (int i = 0; i < len; i++) {
        CT_IIC_Send_Byte(buf[i]);
        ret = CT_IIC_Wait_Ack();
        if (ret) break;
    }
    CT_IIC_Stop();
    return ret;
}

/* ---- 读寄存器 ---- */
void GT5663_RD_Reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    CT_IIC_Start();
    CT_IIC_Send_Byte(GT_CMD_WR);
    CT_IIC_Wait_Ack();
    CT_IIC_Send_Byte(reg >> 8);
    CT_IIC_Wait_Ack();
    CT_IIC_Send_Byte(reg & 0xFF);
    CT_IIC_Wait_Ack();
    CT_IIC_Start();
    CT_IIC_Send_Byte(GT_CMD_RD);
    CT_IIC_Wait_Ack();
    for (int i = 0; i < len; i++) {
        buf[i] = CT_IIC_Read_Byte(i == (len - 1) ? 0 : 1);
    }
    CT_IIC_Stop();
}

/* ---- 初始化 GT5663 ---- */
uint8_t GT5663_Init(void)
{
    uint8_t temp[5];
    GPIO_InitTypeDef init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* ── PB1 / PC13 已由 CubeMX MX_GPIO_Init() 正确初始化 ──
     *     PB1  = INPUT + PULLUP  (GT5663 INT 输出脚, MCU 绝不能驱为 OUTPUT!)
     *     PC13 = OUTPUT_PP + PULLUP
     *     此处不重复初始化, 直接进入 I2C 和复位序列
     */

    CT_IIC_Init();

    /* 复位时序 */
    GT_RST_L();
    HAL_Delay(15);
    GT_RST_H();
    HAL_Delay(15);

    /* PB1 切为 INPUT + NOPULL (对齐官方: 复位后切为 AF_NOPULL) */
    init.Pin   = GPIO_PIN_1;
    init.Mode  = GPIO_MODE_INPUT;
    init.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &init);

    HAL_Delay(100);

    /* 读 PID — 先测试 I2C 通信是否正常 */
    GT5663_RD_Reg(GT_PID_REG, temp, 4);
    temp[4] = 0;
    printf("[TOUCH] CTP ID: %02X %02X %02X %02X (\"%s\")\r\n",
           temp[0], temp[1], temp[2], temp[3], temp);

    /* 如果读到全 0 或全 FF → I2C 通信失败 */
    if ((temp[0] == 0x00 && temp[1] == 0x00 && temp[2] == 0x00 && temp[3] == 0x00) ||
        (temp[0] == 0xFF && temp[1] == 0xFF && temp[2] == 0xFF && temp[3] == 0xFF))
    {
        printf("[TOUCH] I2C read all %02X — bus fail! Check SDA/SCL wiring\r\n", temp[0]);
        return 1;
    }

    /* PID 校验 — 放宽匹配: GT5688 返回 "5688", GT911 返回 "911", GT5663 返回 "5663" */
    int pid_ok = (strcmp((char*)temp, "911") == 0) ||
                 (strcmp((char*)temp, "5668") == 0) ||
                 (strcmp((char*)temp, "5663") == 0) ||
                 (strcmp((char*)temp, "5688") == 0) ||
                 (temp[0] >= 0x30 && temp[0] <= 0x39); /* 数字开头就算 OK */
    if (!pid_ok) {
        printf("[TOUCH] Unrecognized PID, trying init anyway...\r\n");
    }

    temp[0] = 0x02;
    GT5663_WR_Reg(GT_CTRL_REG, temp, 1);  /* 软复位 */
    HAL_Delay(10);
    temp[0] = 0x00;
    GT5663_WR_Reg(GT_CTRL_REG, temp, 1);  /* 清除复位, 进入工作模式 */
    printf("[TOUCH] Init done (PID %s)\r\n", pid_ok ? "OK" : "forced");
    return 0;
}

/* ---- 扫描触摸 (LVGL 调用) ----
 *   每次调用都读 GSTID_REG 确保状态实时 (I2C 开销 ~100us, 可忽略)
 *   坐标每 2 次刷新一次, 减少 I2C 总线占用 */
uint8_t GT5663_Scan(void)
{
    uint8_t buf[4], mode;
    static uint8_t tick = 0;

    tick++;

    /* 每次都读状态寄存器 — 确保按下/松开实时响应 */
    GT5663_RD_Reg(GT_GSTID_REG, &mode, 1);

    /* bit7 置位则清状态 (不管有无触摸点 — 关键! 不清会永久卡住) */
    if (mode & 0x80) {
        uint8_t zero = 0;
        GT5663_WR_Reg(GT_GSTID_REG, &zero, 1);
    }

    uint8_t points = mode & 0x0F;
    g_touch.points = points;

    if (points > 0 && points < 6) {
        g_touch.sta = 0xFF;

        /* 坐标每 2 次刷新一次 (首次连续读 4 次确保快速响应) */
        if ((tick & 1) == 0 || tick < 8) {
            for (int i = 0; i < points; i++) {
                GT5663_RD_Reg(TPX_TBL[i], buf, 4);

                uint16_t chip_x = ((uint16_t)buf[1] << 8) | buf[0];
                uint16_t chip_y = ((uint16_t)buf[3] << 8) | buf[2];

                /* 横屏坐标变换 (GT911 专用 — 与 GT5663 轴方向不同):
                 *   芯片 X(短边 0~480) → 显示 Y(直接映射)
                 *   芯片 Y(长边 0~800) → 显示 X(翻转: 800-chip_y) */
                g_touch.y[i] = chip_x;
                g_touch.x[i] = 800 - chip_y;

                /* 边界裁剪 */
                if (g_touch.x[i] >= 800) g_touch.x[i] = 799;
                if (g_touch.y[i] >= 480) g_touch.y[i] = 479;
            }
        }
        return 1;
    }

    /* 无触摸 */
    g_touch.sta = 0;
    return 0;
}
