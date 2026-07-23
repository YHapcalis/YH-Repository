/*
 * font.h — 字体/图模资源类型与声明
 *
 * 在工程网络里：
 * - oled.c 的 OLED_Print* 系列函数通过 Font/ASCIIFont 读取点阵数据并绘制
 * - app.c 通过 Font/ASCIIFont/Image 直接引用字模/图模资源
 */

#ifndef __FONT_H
#define __FONT_H
#include "stdint.h"
#include "string.h"
typedef struct ASCIIFont
{
    uint8_t h;
    uint8_t w;
    uint8_t *chars;
} ASCIIFont;

extern const ASCIIFont afont8x6;
extern const ASCIIFont afont12x6;
extern const ASCIIFont afont16x8;
extern const ASCIIFont afont24x12;

/**
 * @brief 字体结构体
 * @note  字库前4字节存储utf8编码 剩余字节存储字模数据
 * @note 字库数据可以使用波特律动LED取模助手生成(https://led.baud-dance.com)
 */
typedef struct Font
{
    uint8_t h;              // 字高度
    uint8_t w;              // 字宽度
    const uint8_t *chars;   // 字库 字库前4字节存储utf8编码 剩余字节存储字模数据
    uint8_t len;            // 字库长度 超过256则请改为uint16_t
    const ASCIIFont *ascii; // 缺省ASCII字体 当字库中没有对应字符且需要显示ASCII字符时使用
} Font;

/*用户字模数据格式定义区*/
extern const Font font12x12;
extern const Font font16x16;

/**
 * @brief 图片结构体
 * @note  图片数据可以使用波特律动LED取模助手生成(https://led.baud-dance.com)
 */
typedef struct Image
{
    uint8_t w;           // 图片宽度
    uint8_t h;           // 图片高度
    const uint8_t *data; // 图片数据
} Image;

/*用户图模数据定义区*/

#endif // __FONT_H