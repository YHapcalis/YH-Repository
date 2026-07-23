/*
 * oled.h — OLED 能力接口（显存 + 绘图/文字）
 *
 * 关键网络连接点：
 * - main.c 启动阶段通过 OLED_SetWriteCallback() 注入 I2C 发送回调，让 oled.c 与具体总线解耦
 * - app.c / games 按 “OLED_NewFrame -> OLED_DrawXxx/OLED_PrintXxx -> OLED_ShowFrame” 的节拍使用
 */

#ifndef __OLED_H__
#define __OLED_H__

#include "font.h"
#include "main.h"
#include "string.h"

typedef enum
{
    OLED_COLOR_NORMAL = 0, // 正常模式 黑底白字
    OLED_COLOR_REVERSED    // 反色模式 白底黑字
} OLED_ColorMode;

typedef HAL_StatusTypeDef (*OLED_WriteCallback)(uint16_t devAddr,
                                                const uint8_t *data,
                                                uint16_t len,
                                                uint32_t timeoutMs,
                                                void *userCtx);

void OLED_SetWriteCallback(OLED_WriteCallback callback, void *userCtx);
void OLED_SetBusConfig(uint16_t devAddr, uint32_t timeoutMs);

HAL_StatusTypeDef OLED_Init(void);

void OLED_DisPlay_On();
void OLED_DisPlay_Off();

void OLED_NewFrame();

HAL_StatusTypeDef OLED_ShowFrame(void);

void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color);

void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, OLED_ColorMode color);
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color);
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color);
void OLED_DrawTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color);
void OLED_DrawFilledTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color);
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color);
void OLED_DrawFilledCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color);
void OLED_DrawEllipse(uint8_t x, uint8_t y, uint8_t a, uint8_t b, OLED_ColorMode color);
void OLED_DrawImage(uint8_t x, uint8_t y, const Image *img, OLED_ColorMode color);

void OLED_PrintASCIIChar(uint8_t x, uint8_t y, char ch, const ASCIIFont *font, OLED_ColorMode color);
void OLED_PrintASCIIString(uint8_t x, uint8_t y, char *str, const ASCIIFont *font, OLED_ColorMode color);
void OLED_PrintString(uint8_t x, uint8_t y, char *str, const Font *font, OLED_ColorMode color);
/* 按字库索引直接绘制一个字模（不做UTF-8查找） */
void OLED_PrintGlyphByIndex(uint8_t x, uint8_t y, uint8_t glyphIndex, const Font *font, OLED_ColorMode color);
#endif // __OLED_H__