/*
 * ov7670.c — OV7670 摄像头驱动 (官方例程适配版)
 *
 * 与 CubeMX 适配：移除 GPIO/EXTI 重复初始化，保留传感器配置 + SCCB 通信。
 * GPIO 由 MX_GPIO_Init() 完成，EXTI 中断由 stm32f4xx_it.c 处理。
 */

#include "ov7670.h"
#include "system.h"
#include "ov7670cfg.h"
#include "sccb.h"
#include "nt35510.h"
#include <stdio.h>

u8 ov_sta;	/* 帧中断标志 */

/* 初始化 OV7670 (仅传感器寄存器配置，GPIO/EXTI 由 CubeMX 完成) */
u8 OV7670_Init(void)
{
    u8 temp;
    u16 i = 0;

    /* 复位 SCCB 总线 */
    SCCB_Init();

    /* 检测摄像头 ID */
    temp = SCCB_RD_Reg(0x0A);  /* 产品 ID 高字节 */
    if (temp != 0x76) {
        printf("[CAM] OV7670 not found (ID=0x%02X)\r\n", temp);
        return 1;
    }
    temp = SCCB_RD_Reg(0x0B);  /* 产品 ID 低字节 */
    if (temp != 0x73) {
        printf("[CAM] OV7670 not found (ID=0x%02X)\r\n", temp);
        return 2;
    }
    printf("[CAM] OV7670 detected\r\n");

    /* 写入寄存器配置表 */
    for (i = 0; i < sizeof(ov7670_config) / sizeof(ov7670_config[0]); i++) {
        SCCB_WR_Reg(ov7670_config[i][0], ov7670_config[i][1]);
    }

    /* 设置光晕模式 */
    temp = SCCB_RD_Reg(0x13);  /* COM8 */
    if (temp & 0x01)
        SCCB_WR_Reg(0x13, temp | 0x04);  /* 启用手动 AWB */
    else
        SCCB_WR_Reg(0x13, temp);

    return 0;
}

/* 灯光模式 */
void OV7670_Light_Mode(u8 mode)
{
    u8 com8;

    com8 = SCCB_RD_Reg(0x13);  /* COM8 — 关闭 AWB */
    SCCB_WR_Reg(0x13, com8 & 0xFE);

    switch (mode) {
        case 0:  /* Auto */
            SCCB_WR_Reg(0x13, com8 | 0x01);  /* 启用 AWB */
            break;
        case 1:  /* Sunny */
            SCCB_WR_Reg(0x24, 0x5A); SCCB_WR_Reg(0x25, 0x5C);
            SCCB_WR_Reg(0x26, 0x68); SCCB_WR_Reg(0x27, 0x50);
            break;
        case 2:  /* Cloudy */
            SCCB_WR_Reg(0x24, 0x66); SCCB_WR_Reg(0x25, 0x60);
            SCCB_WR_Reg(0x26, 0x70); SCCB_WR_Reg(0x27, 0x60);
            break;
        case 3:  /* Office */
            SCCB_WR_Reg(0x24, 0x54); SCCB_WR_Reg(0x25, 0x68);
            SCCB_WR_Reg(0x26, 0x64); SCCB_WR_Reg(0x27, 0x90);
            break;
        case 4:  /* Home */
            SCCB_WR_Reg(0x24, 0x5C); SCCB_WR_Reg(0x25, 0x68);
            SCCB_WR_Reg(0x26, 0x50); SCCB_WR_Reg(0x27, 0x80);
            break;
    }
}

/* 色彩饱和度 */
void OV7670_Color_Saturation(u8 sat)
{
    /* uvcol = sat */
    u8 com13 = SCCB_RD_Reg(0x3D);
    com13 &= 0xFE;
    SCCB_WR_Reg(0x3D, com13);
    if (sat <= 4) {
        static const u8 table[5][2] = {
            {0x40, 0x92}, {0x52, 0xB4}, {0x62, 0xD6},
            {0x72, 0xF8}, {0x82, 0x1A}
        };
        SCCB_WR_Reg(0x58, table[sat][0]);
        SCCB_WR_Reg(0x59, table[sat][1]);
    }
}

/* 亮度 */
void OV7670_Brightness(u8 bright)
{
    SCCB_WR_Reg(0x55, 0x00);
    s8 val = (s8)bright - 4;  /* 0~8 映射到 -4~+4 */
    SCCB_WR_Reg(0x56, (u8)(val * 6));
}

/* 对比度 */
void OV7670_Contrast(u8 contrast)
{
    SCCB_WR_Reg(0x3A, 0x04);
    static const u8 table[5] = {0x30, 0x3C, 0x48, 0x54, 0x60};
    if (contrast <= 4)
        SCCB_WR_Reg(0x68, table[contrast]);
}

/* 特效 */
void OV7670_Special_Effects(u8 eft)
{
    switch (eft) {
        case 0:  /* Normal */
            SCCB_WR_Reg(0x3D, 0x80); break;
        case 1:  /* Negative */
            SCCB_WR_Reg(0x3D, 0x86); break;
        case 2:  /* B&W */
            SCCB_WR_Reg(0x3D, 0x82); break;
        case 3:  /* Redish */
            SCCB_WR_Reg(0x3D, 0x83); SCCB_WR_Reg(0x59, 0x00); break;
        case 4:  /* Greenish */
            SCCB_WR_Reg(0x3D, 0x83); SCCB_WR_Reg(0x59, 0x7F); break;
        case 5:  /* Bluish */
            SCCB_WR_Reg(0x3D, 0x83); SCCB_WR_Reg(0x59, 0xFE); break;
        case 6:  /* Antique */
            SCCB_WR_Reg(0x3D, 0x84); break;
    }
}

/* 设置窗口 */
void OV7670_Window_Set(u16 sx, u16 sy, u16 width, u16 height)
{
    SCCB_WR_Reg(0x17, sx & 0xFF);       /* HSTART */
    SCCB_WR_Reg(0x18, (sx & 0x300) >> 2 | (width & 0x300) >> 4); /* HSTOP */
    SCCB_WR_Reg(0x19, sy & 0xFF);       /* VSTART */
    SCCB_WR_Reg(0x1A, (sy & 0x300) >> 2 | (height & 0x300) >> 4); /* VSTOP */
    width &= 0x3FF;
    height &= 0x3FF;
    SCCB_WR_Reg(0x70, SCCB_RD_Reg(0x70) & 0x7F);  /* 启用自定义窗口 */
}

/* 摄像头刷新 (从 FIFO 读取一帧并写入 LCD) */
void camera_refresh(void)
{
    if (!ov_sta) return;

    u32 j;
    u16 color;

    /* 直接写 LCD (400x240 窗口, 居中于 800x480) */
    NT35510_SetWindow(200, 120, 400, 240);

    OV7670_RRST = 0;
    OV7670_RCK_L;
    OV7670_RCK_H;
    OV7670_RCK_L;
    OV7670_RRST = 1;
    OV7670_RCK_H;

    for (j = 0; j < 76800; j++) {
        OV7670_RCK_L;
        color = OV7670_DATA & 0xFF;
        OV7670_RCK_H;
        color <<= 8;
        OV7670_RCK_L;
        color |= OV7670_DATA & 0xFF;
        OV7670_RCK_H;
        NT35510_WritePixel(color);
    }

    ov_sta = 0;
}

/* NT35510 驱动 (用于 camera_refresh 直接写屏) */
#include "nt35510.h"
