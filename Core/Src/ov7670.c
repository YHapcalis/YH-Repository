#include "ov7670.h"
#include "ov7670cfg.h"
#include "sccb.h"
#include "delay.h"
#include "nt35510.h"

/*
 * OV7670 摄像头驱动 + AL422B FIFO 控制
 *
 * 中断处理模型:
 *   VSYNC 上升沿触发 EXTI9_5_IRQHandler
 *   第1个 VSYNC: 复位 FIFO 写指针, 使能 FIFO 写入
 *   第2个 VSYNC: 禁止 FIFO 写入, 复位写指针
 *   ov_sta==2 时主循环读取 FIFO 数据
 *
 * DWT 延时:
 *   使用 DWT 数据观察点跟踪单元的 CYCCNT 计数器
 *   (168MHz -> 1us = 168 个内核时钟周期)
 *   不与 HAL SysTick 冲突
 */

/* ---------------------------------------------------------------------------
 * 帧中断状态
 *   ov_sta  -- 帧捕获状态机
 *   ov_frame -- 帧计数器 (用于调试/测帧率)
 * --------------------------------------------------------------------------- */
volatile u8 ov_sta   = 0;    /* 0=等待, 1=帧进行中, 2=帧就绪 */
volatile u8 ov_frame = 0;

/* ---------------------------------------------------------------------------
 * DWT 微秒延时已在 delay_service.c 中统一实现,
 * ov7670.c 和 sccb.c 均使用 delay_us() 接口。
 * --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * EXTI 中断处理已在 stm32f4xx_it.c 中实现 (#ifndef LFS_READONLY 隔离),
 * ov7670.c 不再重复定义中断函数, 避免链接冲突.
 * --------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * OV7670 GPIO 初始化 (HAL 库版本)
 *
 * 引脚分配 (与 任务说明.md §2.3 一致):
 *   PA4  — RRST     Output PP
 *   PA6  — RCK      Output PP
 *   PA8  — VSYNC    EXTI 上升沿
 *   PB6  — D5       Input  (数据位 5)
 *   PB7  — WRST     Output PP
 *   PC6  — D0       Input
 *   PC7  — D1       Input
 *   PC8  — D2       Input
 *   PC9  — D3       Input
 *   PC11 — D4       Input
 *   PD6  — SCCB_SCL 在 sccb.c 中初始化
 *   PD7  — SCCB_SDA 在 sccb.c 中初始化
 *   PE5  — D6       Input
 *   PE6  — D7       Input
 *   PG9  — WREN     Output PP
 *   PG15 — CS/OE    Output PP
 * --------------------------------------------------------------------------- */
static void OV7670_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能所有需要用到的 GPIO 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /*
     * 输出引脚 (控制信号)
     */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;  /* ~100MHz */
    GPIO_InitStruct.Pull  = GPIO_PULLUP;

    /* PA4 (RRST), PA6 (RCK) */
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_6, GPIO_PIN_SET);

    /* PB7 (WRST) */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);

    /* PG9 (WREN), PG15 (CS/OE) */
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_9 | GPIO_PIN_15, GPIO_PIN_SET);

    /*
     * 输入引脚 (数据 + VSYNC)
     */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;

    /* PA8 (VSYNC) — 先配为输入, EXTI 稍后配置 */
    GPIO_InitStruct.Pin  = GPIO_PIN_8;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PC6,7,8,9,11 — 数据位 D0~D4 */
    GPIO_InitStruct.Pin  = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_11;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;   /* 数据总线: 内部下拉, 避免浮空 */
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* PB6 — 数据位 D5 */
    GPIO_InitStruct.Pin  = GPIO_PIN_6;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PE5, PE6 — 数据位 D6, D7 */
    GPIO_InitStruct.Pin  = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

/* ---------------------------------------------------------------------------
 * EXTI + NVIC 初始化 (PA8 VSYNC, 上升沿触发)
 * --------------------------------------------------------------------------- */
static void OV7670_EXTI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 使能 SYSCFG (用于 EXTI 配置) */
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    /* PA8 配置为 EXTI 上升沿中断 */
    GPIO_InitStruct.Pin   = GPIO_PIN_8;
    GPIO_InitStruct.Mode  = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* NVIC 配置: 最高优先级 (抢占 0, 子 0) */
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

/* ---------------------------------------------------------------------------
 * OV7670 初始化 (主入口)
 *
 * 返回值:  0 = 成功
 *          1 = SCCB 复位失败
 *          2 = 芯片 ID 不匹配 (期待 0x73/0x76)
 * --------------------------------------------------------------------------- */
u8 OV7670_Init(void)
{
    u8 temp;
    u16 i;

    /* 延时服务初始化 */
    delay_init();

    /* 1. GPIO 初始化 (所有摄像头引脚) */
    OV7670_GPIO_Init();

    /* 2. EXTI + NVIC 初始化 (PA8 VSYNC) */
    OV7670_EXTI_Init();

    /* 3. SCCB 总线初始化 */
    SCCB_Init();

    /* 4. SCCB 复位摄像头 */
    if (SCCB_WR_Reg(0x12, 0x80)) return 1;  /* COM7 复位 */
    HAL_Delay(50);

    /* 5. 读取芯片 ID 验证 */
    temp = SCCB_RD_Reg(0x0b);    /* Manufacturer ID (高字节) */
    if (temp != 0x73) return 2;
    temp = SCCB_RD_Reg(0x0a);    /* Manufacturer ID (低字节) */
    if (temp != 0x76) return 2;

    /* 6. 遍历配置表写入寄存器 */
    for (i = 0; i < sizeof(ov7670_init_reg_tbl) / sizeof(ov7670_init_reg_tbl[0]); i++) {
        SCCB_WR_Reg(ov7670_init_reg_tbl[i][0], ov7670_init_reg_tbl[i][1]);
    }

    return 0;   /* OK */
}

/* ---------------------------------------------------------------------------
 * 效果调节函数 (均通过 SCCB 写寄存器)
 * 可在主循环中动态调用
 * --------------------------------------------------------------------------- */

/* 灯光模式: 0=自动, 1=晴天, 2=阴天, 3=办公室, 4=家庭 */
void OV7670_Light_Mode(u8 mode)
{
    u8 reg13val = 0xE7;
    u8 reg01val = 0;
    u8 reg02val = 0;

    switch (mode) {
        case 1: reg13val = 0xE5; reg01val = 0x5A; reg02val = 0x5C; break; /* Sunny    */
        case 2: reg13val = 0xE5; reg01val = 0x58; reg02val = 0x60; break; /* Cloudy   */
        case 3: reg13val = 0xE5; reg01val = 0x84; reg02val = 0x4c; break; /* Office   */
        case 4: reg13val = 0xE5; reg01val = 0x96; reg02val = 0x40; break; /* Home     */
        default: break;                                                     /* Auto     */
    }
    SCCB_WR_Reg(0x13, reg13val);
    SCCB_WR_Reg(0x01, reg01val);
    SCCB_WR_Reg(0x02, reg02val);
}

/* 色饱和度: 0(-2)~4(+2), 默认 2 */
void OV7670_Color_Saturation(u8 sat)
{
    u8 reg4f5054val = 0x80;
    u8 reg52val = 0x22;
    u8 reg53val = 0x5E;

    switch (sat) {
        case 0: reg4f5054val = 0x40; reg52val = 0x11; reg53val = 0x2F; break;
        case 1: reg4f5054val = 0x66; reg52val = 0x1B; reg53val = 0x4B; break;
        case 3: reg4f5054val = 0x99; reg52val = 0x28; reg53val = 0x71; break;
        case 4: reg4f5054val = 0xC0; reg52val = 0x33; reg53val = 0x8D; break;
        default: break;
    }
    SCCB_WR_Reg(0x4F, reg4f5054val);
    SCCB_WR_Reg(0x50, reg4f5054val);
    SCCB_WR_Reg(0x51, 0x00);
    SCCB_WR_Reg(0x52, reg52val);
    SCCB_WR_Reg(0x53, reg53val);
    SCCB_WR_Reg(0x54, reg4f5054val);
    SCCB_WR_Reg(0x58, 0x9E);
}

/* 亮度: 0(-2)~4(+2), 默认 2 */
void OV7670_Brightness(u8 bright)
{
    u8 reg55val = 0x00;

    switch (bright) {
        case 0: reg55val = 0xB0; break;
        case 1: reg55val = 0x98; break;
        case 3: reg55val = 0x18; break;
        case 4: reg55val = 0x30; break;
        default: break;
    }
    SCCB_WR_Reg(0x55, reg55val);
}

/* 对比度: 0(-2)~4(+2), 默认 2 */
void OV7670_Contrast(u8 contrast)
{
    u8 reg56val = 0x40;

    switch (contrast) {
        case 0: reg56val = 0x30; break;
        case 1: reg56val = 0x38; break;
        case 3: reg56val = 0x50; break;
        case 4: reg56val = 0x60; break;
        default: break;
    }
    SCCB_WR_Reg(0x56, reg56val);
}

/* 特效: 0=普通, 1=负片, 2=黑白, 3=偏红, 4=偏绿, 5=偏蓝, 6=复古 */
void OV7670_Special_Effects(u8 eft)
{
    u8 reg3aval = 0x04;
    u8 reg67val = 0xC0;
    u8 reg68val = 0x80;

    switch (eft) {
        case 1: reg3aval = 0x24; reg67val = 0x80; reg68val = 0x80; break; /* Negative */
        case 2: reg3aval = 0x14; reg67val = 0x80; reg68val = 0x80; break; /* B&W      */
        case 3: reg3aval = 0x14; reg67val = 0xC0; reg68val = 0x80; break; /* Reddish  */
        case 4: reg3aval = 0x14; reg67val = 0x40; reg68val = 0x40; break; /* Greenish */
        case 5: reg3aval = 0x14; reg67val = 0x80; reg68val = 0xC0; break; /* Bluish   */
        case 6: reg3aval = 0x14; reg67val = 0xA0; reg68val = 0x40; break; /* Antique  */
        default: break;
    }
    SCCB_WR_Reg(0x3A, reg3aval);
    SCCB_WR_Reg(0x68, reg67val);
    SCCB_WR_Reg(0x67, reg68val);
}

/* 窗口设置: QVGA 时使用 (12,176,240,320) */
void OV7670_Window_Set(u16 sx, u16 sy, u16 width, u16 height)
{
    u16 endx;
    u16 endy;
    u8  temp;

    endx = sx + width * 2;
    endy = sy + height * 2;
    if (endy > 784) endy -= 784;

    temp = SCCB_RD_Reg(0x03);
    temp &= 0xF0;
    temp |= ((endx & 0x03) << 2) | (sx & 0x03);
    SCCB_WR_Reg(0x03, temp);
    SCCB_WR_Reg(0x19, sx >> 2);
    SCCB_WR_Reg(0x1A, endx >> 2);

    temp = SCCB_RD_Reg(0x32);
    temp &= 0xC0;
    temp |= ((endy & 0x07) << 3) | (sy & 0x07);
    SCCB_WR_Reg(0x17, sy >> 3);
    SCCB_WR_Reg(0x18, endy >> 3);
}

/* ---------------------------------------------------------------------------
 * camera_refresh — 从 AL422B FIFO 读取一帧数据并写入 LCD (NT35510)
 *
 * 前置条件:
 *   ov_sta >= 2 (一帧 FIFO 数据已就绪)
 *   窗口设置为 QVGA 320×240 居中于 800×480
 * --------------------------------------------------------------------------- */
void camera_refresh(void)
{
    u32 j;
    u16 color;

    if (ov_sta >= 2)     /* 必须 ov_sta==2 (帧已完整写入 FIFO) 才读取, 避免撕裂 */
    {
        /* 设置 LCD 窗口: 居中显示 QVGA 320×240
         * 注意: NT35510_SetWindow 的 width/height 参数实际传终点坐标 */
        uint16_t lcd_w = NT35510_GetWidth();   /* 横屏 = 800 */
        uint16_t lcd_h = NT35510_GetHeight();  /* 横屏 = 480 */
        NT35510_SetWindow((lcd_w - 320) / 2, (lcd_h - 240) / 2,
                          (lcd_w + 320) / 2 - 1, (lcd_h + 240) / 2 - 1);

        /* 复位 FIFO 读指针 */
        OV7670_RRST = 0;
        OV7670_RCK_L;
        OV7670_RCK_H;
        OV7670_RCK_L;
        OV7670_RRST = 1;
        OV7670_RCK_H;

        /* 读取 76800 个像素 (QVGA 320×240, RGB565, 2 字节/像素)
         * 使用寄存器级 IDR 批量读代替位带操作, ~5x 提速
         *
         * 注意: AL422B FIFO 从 RCK↓ 到数据输出有效需 ~20ns (tAC)
         *       加 NOP 等待确保数据稳定后再读 */
        for (j = 0; j < 76800; j++) {
            OV7670_RCK_L;
            __NOP(); __NOP(); __NOP();             /* ~18ns @ 168MHz */
            color  = ov7670_read_byte();            /* 低字节 */
            OV7670_RCK_H;
            color <<= 8;
            OV7670_RCK_L;
            __NOP(); __NOP(); __NOP();
            color |= ov7670_read_byte();            /* 高字节 */
            OV7670_RCK_H;

            NT35510_WritePixel(color);
        }

        ov_sta  = 0;     /* 复位帧状态 */
        ov_frame++;
    }
}
