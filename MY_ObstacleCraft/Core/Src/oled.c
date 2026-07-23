/**
 * @file oled.c
 * @brief 波特律动OLED驱动(CH1116)
 * @anchor 波特律动(keysking 博哥在学习)
 * @version 1.0
 * @date 2023-08-04
 * @license MIT License
 *
 * @attention
 * 本驱动库针对波特律动·keysking的STM32教程学习套件进行开发
 * 在其他平台或驱动芯片上使用可能需要进行移植
 *
 * @note
 * 使用流程:
 * 1. STM32初始化IIC完成后调用OLED_Init()初始化OLED. 注意STM32启动比OLED上电快, 可等待20ms再初始化OLED
 * 2. 调用OLED_NewFrame()开始绘制新的一帧
 * 3. 调用OLED_DrawXXX()系列函数绘制图形到显存 调用OLED_Printxxx()系列函数绘制文本到显存
 * 4. 调用OLED_ShowFrame()将显存内容显示到OLED
 *
 * @note
 * 为保证中文显示正常 请将编译器的字符集设置为UTF-8
 *
 */

/*
 * 【工程逻辑网络·显示/绘图库】oled.c
 *
 * 在本工程里，oled.c 是“能力层/画笔”，被 app.c 和 应用 反复调用：
 * - 业务侧（Core/Src/app.c / Core/Src/GreedySnake.c / Core/Src/dinosaur.c）只负责往显存画
 * - 真正的 I2C 发送不在这里写死：Core/Src/main.c 通过 OLED_SetWriteCallback() 注入发送回调（Main_OLEDI2CWrite）
 *
 * 一帧的标准节拍（在 app/game 里反复出现）：
 * - OLED_NewFrame() 清空显存 -> OLED_DrawXxx/OLED_PrintXxx 画到显存 -> OLED_ShowFrame() 刷到屏幕
 *
 * 总线参数：
 * - OLED_SetBusConfig(devAddr, timeoutMs) 由 main.c 在启动阶段设置（本工程默认 0x7A + 20ms）
 */

#include "oled.h"
#include <math.h>
#include <stdlib.h>

// OLED默认总线参数
#define OLED_DEFAULT_ADDRESS 0x7AU
#define OLED_DEFAULT_I2C_TIMEOUT_MS 20U

// OLED参数
#define OLED_PAGE 8            // OLED页数
#define OLED_ROW 8 * OLED_PAGE // OLED行数
#define OLED_COLUMN 128        // OLED列数

// 显存
uint8_t OLED_GRAM[OLED_PAGE][OLED_COLUMN];

static OLED_WriteCallback s_writeCallback = 0;                // 底层发送回调函数指针：由外部 main 绑定
static void *s_writeUserCtx = 0;                              // 用户上下文指针：回调透传，可用于传 I2C 句柄等
static uint16_t s_devAddr = OLED_DEFAULT_ADDRESS;             // 设备地址：默认 0x7A（8位地址写法）
static uint32_t s_i2cTimeoutMs = OLED_DEFAULT_I2C_TIMEOUT_MS; // I2C超时：单位 ms

void OLED_SetWriteCallback(OLED_WriteCallback callback, void *userCtx)
{
    s_writeCallback = callback; // 保存函数指针：后续 OLED_Send 会通过它真正发数据
    s_writeUserCtx = userCtx;   // 保存上下文：不在驱动层解释，原样传给回调
}

void OLED_SetBusConfig(uint16_t devAddr, uint32_t timeoutMs)
{
    s_devAddr = devAddr;        // 更新设备地址（例如更换模块地址时可重设）
    s_i2cTimeoutMs = timeoutMs; // 更新超时参数（慢速总线可适当放宽）
}

// ========================== 底层通信函数 ==========================

/**
 * @brief 向OLED发送数据的函数
 * @param data 要发送的数据
 * @param len 要发送的数据长度
 * @return None
 * @note 此函数是移植本驱动时的重要函数 将本驱动库移植到其他平台时应根据实际情况修改此函数
 */
static HAL_StatusTypeDef OLED_Send(const uint8_t *data, uint16_t len) // OLED发送函数，使用注入回调发送到OLED，返回HAL状态码
{
    if (s_writeCallback == 0) // 尚未绑定底层发送函数：无法通信
    {
        return HAL_ERROR; // 返回错误，提醒上层先调用 OLED_SetWriteCallback
    }
    return s_writeCallback(s_devAddr, data, len, s_i2cTimeoutMs, s_writeUserCtx); // 通过回调发出整帧/命令
}

/**
 * @brief 向OLED发送指令
 */
static HAL_StatusTypeDef OLED_SendCmd(uint8_t cmd)
{
    static uint8_t sendBuffer[2] = {0}; // sendBuffer[0]=0x00(命令控制字), sendBuffer[1]=命令值
    sendBuffer[1] = cmd;                // 每次只替换第2字节即可
    return OLED_Send(sendBuffer, 2);    // 统一走 OLED_Send，保证底层发送路径一致
}

// ========================== OLED驱动函数 ==========================

/**
 * @brief 初始化OLED
 * @note 此函数是移植本驱动时的重要函数 将本驱动库移植到其他驱动芯片时应根据实际情况修改此函数
 */
HAL_StatusTypeDef OLED_Init(void)
{
    if (s_writeCallback == 0) // 安全检查：没有绑定回调就不能初始化
    {
        return HAL_ERROR;
    }

    if (OLED_SendCmd(0xAE) != HAL_OK) /*关闭显示 display off*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x02) != HAL_OK) /*设置列起始地址 set lower column address*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x10) != HAL_OK) /*设置列结束地址 set higher column address*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x40) != HAL_OK) /*设置起始行 set display start line*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xB0) != HAL_OK) /*设置页地址 set page address*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x81) != HAL_OK) /*设置对比度 contract control*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xCF) != HAL_OK) /*128*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xA1) != HAL_OK) /*设置分段重映射 从右到左 set segment remap*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xA6) != HAL_OK) /*正向显示 normal / reverse*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xA8) != HAL_OK) /*多路复用率 multiplex ratio*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x3F) != HAL_OK) /*duty = 1/64*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xAD) != HAL_OK) /*设置启动电荷泵 set charge pump enable*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x8B) != HAL_OK) /*启动DC-DC */
        return HAL_ERROR;

    if (OLED_SendCmd(0x33) != HAL_OK) /*设置泵电压 set VPP 10V */
        return HAL_ERROR;

    if (OLED_SendCmd(0xC8) != HAL_OK) /*设置输出扫描方向 COM[N-1]到COM[0] Com scan direction*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xD3) != HAL_OK) /*设置显示偏移 set display offset*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x00) != HAL_OK) /*0x00*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xD5) != HAL_OK) /*设置内部时钟频率 set osc frequency*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xC0) != HAL_OK)
        return HAL_ERROR;

    if (OLED_SendCmd(0xD9) != HAL_OK) /*设置放电/预充电时间 set pre-charge period*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x1F) != HAL_OK) /*0x22*/
        return HAL_ERROR;

    if (OLED_SendCmd(0xDA) != HAL_OK) /*设置引脚布局 set COM pins*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x12) != HAL_OK)
        return HAL_ERROR;

    if (OLED_SendCmd(0xDB) != HAL_OK) /*设置电平 set vcomh*/
        return HAL_ERROR;

    if (OLED_SendCmd(0x40) != HAL_OK)
        return HAL_ERROR;

    OLED_NewFrame(); // 清空显存缓存（RAM）

    if (OLED_ShowFrame() != HAL_OK) // 把空白帧刷到屏幕，避免上电残影
        return HAL_ERROR;

    if (OLED_SendCmd(0xAF) != HAL_OK) /*开启显示 display on*/
        return HAL_ERROR;

    return HAL_OK;
}

/**
 * @brief 开启OLED显示
 */
void OLED_DisPlay_On()
{
    OLED_SendCmd(0x8D); // 电荷泵使能
    OLED_SendCmd(0x14); // 开启电荷泵
    OLED_SendCmd(0xAF); // 点亮屏幕
}

/**
 * @brief 关闭OLED显示
 */
void OLED_DisPlay_Off()
{
    OLED_SendCmd(0x8D); // 电荷泵使能
    OLED_SendCmd(0x10); // 关闭电荷泵
    OLED_SendCmd(0xAE); // 关闭屏幕
}

/**
 * @brief 设置颜色模式 黑底白字或白底黑字
 * @param ColorMode 颜色模式COLOR_NORMAL/COLOR_REVERSED
 * @note 此函数直接设置屏幕的颜色模式
 */
void OLED_SetColorMode(OLED_ColorMode mode)
{
    if (mode == OLED_COLOR_NORMAL)
    {
        OLED_SendCmd(0xA6); // 正常显示
    }
    if (mode == OLED_COLOR_REVERSED)
    {
        OLED_SendCmd(0xA7); // 反色显示
    }
}

// ========================== 显存操作函数 ==========================

/**
 * @brief 清空显存 绘制新的一帧
 */
void OLED_NewFrame()
{
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM)); // memset: 把整块显存缓冲全部置 0（清屏缓存）
}

/**
 * @brief 将当前显存显示到屏幕上
 * @note 此函数是移植本驱动时的重要函数 将本驱动库移植到其他驱动芯片时应根据实际情况修改此函数
 */
HAL_StatusTypeDef OLED_ShowFrame(void) // OLED显示函数，将显存内容通过I2C发送到OLED显示，返回HAL状态码
{
    static uint8_t sendBuffer[OLED_COLUMN + 1]; // +1 给控制字节：0x40 表示后续是数据流
    sendBuffer[0] = 0x40;                       // 数据控制字：Co=0, D/C#=1
    for (uint8_t i = 0; i < OLED_PAGE; i++)     // 逐页刷新：每页 8 像素高
    {
        if (OLED_SendCmd(0xB0 + i) != HAL_OK) // 设置页地址
            return HAL_ERROR;

        if (OLED_SendCmd(0x02) != HAL_OK) // 设置列地址低4位
            return HAL_ERROR;

        if (OLED_SendCmd(0x10) != HAL_OK) // 设置列地址高4位
            return HAL_ERROR;

        memcpy(sendBuffer + 1, OLED_GRAM[i], OLED_COLUMN); // 把当前页 128 字节像素数据拷到发送缓冲

        if (OLED_Send(sendBuffer, OLED_COLUMN + 1) != HAL_OK) // 发送数据
            return HAL_ERROR;
    }
    return HAL_OK;
}

/**
 * @brief 设置一个像素点
 * @param x 横坐标
 * @param y 纵坐标
 * @param color 颜色
 */
void OLED_SetPixel(uint8_t x, uint8_t y, OLED_ColorMode color)
{
    if (x >= OLED_COLUMN || y >= OLED_ROW) // 越界保护：坐标不在屏幕范围就直接返回
        return;
    if (!color) // color==0：按本驱动约定表示“点亮”
    {
        OLED_GRAM[y / 8][x] |= 1 << (y % 8); // 定位到页和位后，按位或置 1
    }
    else
    {
        OLED_GRAM[y / 8][x] &= ~(1 << (y % 8)); // 按位与清零，实现灭点
    }
}

/**
 * @brief 设置显存中一字节数据的某几位
 * @param page 页地址
 * @param column 列地址
 * @param data 数据
 * @param start 起始位
 * @param end 结束位
 * @param color 颜色
 * @note 此函数将显存中的某一字节的第start位到第end位设置为与data相同
 * @note start和end的范围为0-7, start必须小于等于end
 * @note 此函数与OLED_SetByte_Fine的区别在于此函数只能设置显存中的某一真实字节
 */
void OLED_SetByte_Fine(uint8_t page, uint8_t column, uint8_t data, uint8_t start, uint8_t end, OLED_ColorMode color)
{
    static uint8_t temp;                            // 临时掩码变量：复用静态变量避免重复开栈
    if (page >= OLED_PAGE || column >= OLED_COLUMN) // 边界保护：页/列越界直接返回
        return;
    if (color) // 反色模式：把输入数据按位取反
        data = ~data;

    temp = data | (0xff << (end + 1)) | (0xff >> (8 - start));   // 保留目标位段外的原始位
    OLED_GRAM[page][column] &= temp;                             // 先清目标位段（通过掩码）
    temp = data & ~(0xff << (end + 1)) & ~(0xff >> (8 - start)); // 提取目标位段的新数据
    OLED_GRAM[page][column] |= temp;                             // 再把新位段写回
    // 使用OLED_SetPixel实现
    // for (uint8_t i = start; i <= end; i++) {
    //   OLED_SetPixel(column, page * 8 + i, !((data >> i) & 0x01));
    // }
}

/**
 * @brief 设置显存中的一字节数据
 * @param page 页地址
 * @param column 列地址
 * @param data 数据
 * @param color 颜色
 * @note 此函数将显存中的某一字节设置为data的值
 */
void OLED_SetByte(uint8_t page, uint8_t column, uint8_t data, OLED_ColorMode color)
{
    if (page >= OLED_PAGE || column >= OLED_COLUMN) // 参数越界则不写显存
        return;
    if (color) // 反色模式
        data = ~data;
    OLED_GRAM[page][column] = data; // 直接覆盖该字节
}

/**
 * @brief 设置显存中的一字节数据的某几位
 * @param x 横坐标
 * @param y 纵坐标
 * @param data 数据
 * @param len 位数
 * @param color 颜色
 * @note 此函数将显存中从(x,y)开始向下数len位设置为与data相同
 * @note len的范围为1-8
 * @note 此函数与OLED_SetByte_Fine的区别在于此函数的横坐标和纵坐标是以像素为单位的, 可能出现跨两个真实字节的情况(跨页)
 */
void OLED_SetBits_Fine(uint8_t x, uint8_t y, uint8_t data, uint8_t len, OLED_ColorMode color)
{
    uint8_t page = y / 8; // 目标起点所在页
    uint8_t bit = y % 8;  // 目标起点在页内的 bit 偏移
    if (bit + len > 8)    // 若超出当前页，说明会跨页
    {
        OLED_SetByte_Fine(page, x, data << bit, bit, 7, color);                         // 上半部分写当前页
        OLED_SetByte_Fine(page + 1, x, data >> (8 - bit), 0, len + bit - 1 - 8, color); // 下半部分写下一页
    }
    else
    {
        OLED_SetByte_Fine(page, x, data << bit, bit, bit + len - 1, color); // 不跨页：一次写完
    }
    // 使用OLED_SetPixel实现
    // for (uint8_t i = 0; i < len; i++) {
    //   OLED_SetPixel(x, y + i, !((data >> i) & 0x01));
    // }
}

/**
 * @brief 设置显存中一字节长度的数据
 * @param x 横坐标
 * @param y 纵坐标
 * @param data 数据
 * @param color 颜色
 * @note 此函数将显存中从(x,y)开始向下数8位设置为与data相同
 * @note 此函数与OLED_SetByte的区别在于此函数的横坐标和纵坐标是以像素为单位的, 可能出现跨两个真实字节的情况(跨页)
 */
void OLED_SetBits(uint8_t x, uint8_t y, uint8_t data, OLED_ColorMode color)
{
    uint8_t page = y / 8;                                   // 起始页
    uint8_t bit = y % 8;                                    // 起始 bit
    OLED_SetByte_Fine(page, x, data << bit, bit, 7, color); // 先写当前页剩余位
    if (bit)                                                // 如果 bit!=0，说明一定跨到下一页
    {
        OLED_SetByte_Fine(page + 1, x, data >> (8 - bit), 0, bit - 1, color); // 补写下一页低位
    }
}

/**
 * @brief 设置一块显存区域
 * @param x 起始横坐标
 * @param y 起始纵坐标
 * @param data 数据的起始地址
 * @param w 宽度
 * @param h 高度
 * @param color 颜色
 * @note 此函数将显存中从(x,y)开始的w*h个像素设置为data中的数据
 * @note data的数据应该采用列行式排列
 */
void OLED_SetBlock(uint8_t x, uint8_t y, const uint8_t *data, uint8_t w, uint8_t h, OLED_ColorMode color)
{
    uint8_t fullRow = h / 8;        // 完整的行数
    uint8_t partBit = h % 8;        // 不完整的字节中的有效位数
    for (uint8_t i = 0; i < w; i++) // i 遍历宽度（列）
    {
        for (uint8_t j = 0; j < fullRow; j++) // j 遍历完整字节行
        {
            OLED_SetBits(x + i, y + j * 8, data[i + j * w], color); // 列行式索引：i + j*w
        }
    }
    if (partBit) // 若高度不是 8 的倍数，需要处理末尾残留位
    {
        uint16_t fullNum = w * fullRow; // 完整的字节数
        for (uint8_t i = 0; i < w; i++)
        {
            OLED_SetBits_Fine(x + i, y + (fullRow * 8), data[fullNum + i], partBit, color); // 仅写 partBit 位
        }
    }
    // 使用OLED_SetPixel实现
    // for (uint8_t i = 0; i < w; i++) {
    //   for (uint8_t j = 0; j < h; j++) {
    //     for (uint8_t k = 0; k < 8; k++) {
    //       if (j * 8 + k >= h) break; // 防止越界(不完整的字节
    //       OLED_SetPixel(x + i, y + j * 8 + k, !((data[i + j * w] >> k) & 0x01));
    //     }
    //   }
    // }
}

// ========================== 图形绘制函数 ==========================
/**
 * @brief 绘制一条线段
 * @param x1 起始点横坐标
 * @param y1 起始点纵坐标
 * @param x2 终止点横坐标
 * @param y2 终止点纵坐标
 * @param color 颜色
 * @note 此函数使用Bresenham算法绘制线段
 */
void OLED_DrawLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, OLED_ColorMode color)
{
    static uint8_t temp = 0; // 临时交换变量：用于起止点排序
    if (x1 == x2)            // 特例1：竖直线
    {
        if (y1 > y2) // 若起点在下方，交换为“从上到下”
        {
            temp = y1;
            y1 = y2;
            y2 = temp;
        }
        for (uint8_t y = y1; y <= y2; y++) // 逐点绘制竖线
        {
            OLED_SetPixel(x1, y, color);
        }
    }
    else if (y1 == y2) // 特例2：水平线
    {
        if (x1 > x2) // 若起点在右侧，交换为“从左到右”
        {
            temp = x1;
            x1 = x2;
            x2 = temp;
        }
        for (uint8_t x = x1; x <= x2; x++) // 逐点绘制横线
        {
            OLED_SetPixel(x, y1, color);
        }
    }
    else
    {
        // Bresenham画线算法（只用整数运算，速度快）
        int16_t dx = x2 - x1;             // 原始 x 增量
        int16_t dy = y2 - y1;             // 原始 y 增量
        int16_t ux = ((dx > 0) << 1) - 1; // ux 为 x 步进方向：+1 或 -1
        int16_t uy = ((dy > 0) << 1) - 1; // uy 为 y 步进方向：+1 或 -1
        int16_t x = x1, y = y1, eps = 0;  // 当前绘制点与误差项
        dx = abs(dx);                     // 绝对值：只保留长度
        dy = abs(dy);                     // 绝对值：只保留长度
        if (dx > dy)                      // x 主导：每次先走 x，再按误差决定何时走 y
        {
            for (x = x1; x != x2; x += ux)
            {
                OLED_SetPixel(x, y, color);
                eps += dy;
                if ((eps << 1) >= dx) // 左移1位等价于 *2
                {
                    y += uy;
                    eps -= dx;
                }
            }
        }
        else
        {
            for (y = y1; y != y2; y += uy) // y 主导：每次先走 y，再按误差决定何时走 x
            {
                OLED_SetPixel(x, y, color);
                eps += dx;
                if ((eps << 1) >= dy)
                {
                    x += ux;
                    eps -= dy;
                }
            }
        }
    }
}

/**
 * @brief 绘制一个矩形
 * @param x 起始点横坐标
 * @param y 起始点纵坐标
 * @param w 矩形宽度
 * @param h 矩形高度
 * @param color 颜色
 */
void OLED_DrawRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color)
{
    OLED_DrawLine(x, y, x + w, y, color);         // 上边
    OLED_DrawLine(x, y + h, x + w, y + h, color); // 下边
    OLED_DrawLine(x, y, x, y + h, color);         // 左边
    OLED_DrawLine(x + w, y, x + w, y + h, color); // 右边
}

/**
 * @brief 绘制一个填充矩形
 * @param x 起始点横坐标
 * @param y 起始点纵坐标
 * @param w 矩形宽度
 * @param h 矩形高度
 * @param color 颜色
 */
void OLED_DrawFilledRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h, OLED_ColorMode color)
{
    for (uint8_t i = 0; i < h; i++) // 逐行画横线来填充
    {
        OLED_DrawLine(x, y + i, x + w, y + i, color);
    }
}

/**
 * @brief 绘制一个三角形
 * @param x1 第一个点横坐标
 * @param y1 第一个点纵坐标
 * @param x2 第二个点横坐标
 * @param y2 第二个点纵坐标
 * @param x3 第三个点横坐标
 * @param y3 第三个点纵坐标
 * @param color 颜色
 */
void OLED_DrawTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color)
{
    OLED_DrawLine(x1, y1, x2, y2, color); // 边1
    OLED_DrawLine(x2, y2, x3, y3, color); // 边2
    OLED_DrawLine(x3, y3, x1, y1, color); // 边3
}

/**
 * @brief 绘制一个填充三角形
 * @param x1 第一个点横坐标
 * @param y1 第一个点纵坐标
 * @param x2 第二个点横坐标
 * @param y2 第二个点纵坐标
 * @param x3 第三个点横坐标
 * @param y3 第三个点纵坐标
 * @param color 颜色
 */
void OLED_DrawFilledTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3, OLED_ColorMode color)
{
    uint8_t a = 0, b = 0, y = 0, last = 0; // 扫描线范围与转折记录
    if (y1 > y2)                           // 先把 y1/y2 排序，a 为较小值，b 为较大值
    {
        a = y2;
        b = y1;
    }
    else
    {
        a = y1;
        b = y2;
    }
    y = a;              // 从上往下扫
    for (; y <= b; y++) // 第一段：与点1-点2、点1-点3 的交点连线
    {
        if (y <= y3) // 还在三角形上半段
        {
            OLED_DrawLine(x1 + (y - y1) * (x2 - x1) / (y2 - y1), y, x1 + (y - y1) * (x3 - x1) / (y3 - y1), y, color);
        }
        else
        {
            last = y - 1; // 记录转折前最后一行
            break;
        }
    }
    for (; y <= b; y++) // 第二段：改用点2-点3 与 点1-last-点3 的交点连线
    {
        OLED_DrawLine(x2 + (y - y2) * (x3 - x2) / (y3 - y2), y, x1 + (y - last) * (x3 - x1) / (y3 - last), y, color);
    }
}

/**
 * @brief 绘制一个圆
 * @param x 圆心横坐标
 * @param y 圆心纵坐标
 * @param r 圆半径
 * @param color 颜色
 * @note 此函数使用Bresenham算法绘制圆
 */
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color)
{
    int16_t a = 0, b = r, di = 3 - (r << 1); // 中点圆算法参数
    while (a <= b)
    {
        // 8个对称点一起画：只算 1/8 圆，其他靠对称补齐
        OLED_SetPixel(x - b, y - a, color);
        OLED_SetPixel(x + b, y - a, color);
        OLED_SetPixel(x - a, y + b, color);
        OLED_SetPixel(x - b, y - a, color);
        OLED_SetPixel(x - a, y - b, color);
        OLED_SetPixel(x + b, y + a, color);
        OLED_SetPixel(x + a, y - b, color);
        OLED_SetPixel(x + a, y + b, color);
        OLED_SetPixel(x - b, y + a, color);
        a++;        // 每轮推进一列
        if (di < 0) // di<0：下一步走“向右”
        {
            di += 4 * a + 6;
        }
        else
        {
            // di>=0：下一步走“右下”
            di += 10 + 4 * (a - b);
            b--;
        }
        OLED_SetPixel(x + a, y + b, color);
    }
}

/**
 * @brief 绘制一个填充圆
 * @param x 圆心横坐标
 * @param y 圆心纵坐标
 * @param r 圆半径
 * @param color 颜色
 * @note 此函数使用Bresenham算法绘制圆
 */
void OLED_DrawFilledCircle(uint8_t x, uint8_t y, uint8_t r, OLED_ColorMode color)
{
    int16_t a = 0, b = r, di = 3 - (r << 1); // 中点圆算法参数
    while (a <= b)
    {
        for (int16_t i = x - b; i <= x + b; i++) // 画上下两条横线（长度 2b）
        {
            OLED_SetPixel(i, y + a, color);
            OLED_SetPixel(i, y - a, color);
        }
        for (int16_t i = x - a; i <= x + a; i++) // 再画另外两条横线（长度 2a）
        {
            OLED_SetPixel(i, y + b, color);
            OLED_SetPixel(i, y - b, color);
        }
        a++;
        if (di < 0)
        {
            di += 4 * a + 6;
        }
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }
    }
}

/**
 * @brief 绘制一个椭圆
 * @param x 椭圆中心横坐标
 * @param y 椭圆中心纵坐标
 * @param a 椭圆长轴
 * @param b 椭圆短轴
 */
void OLED_DrawEllipse(uint8_t x, uint8_t y, uint8_t a, uint8_t b, OLED_ColorMode color)
{
    int xpos = 0, ypos = b;       // 从椭圆最上方开始画
    int a2 = a * a, b2 = b * b;   // 预计算 a^2 / b^2
    int d = b2 + a2 * (0.25 - b); // 区域1判断值初始值
    while (a2 * ypos > b2 * xpos) // 区域1：曲线更“陡”的部分
    {
        // 四象限对称点
        OLED_SetPixel(x + xpos, y + ypos, color);
        OLED_SetPixel(x - xpos, y + ypos, color);
        OLED_SetPixel(x + xpos, y - ypos, color);
        OLED_SetPixel(x - xpos, y - ypos, color);
        if (d < 0) // d<0：选右侧点
        {
            d = d + b2 * ((xpos << 1) + 3);
            xpos += 1;
        }
        else
        {
            // d>=0：选右下点
            d = d + b2 * ((xpos << 1) + 3) + a2 * (-(ypos << 1) + 2);
            xpos += 1, ypos -= 1;
        }
    }
    d = b2 * (xpos + 0.5) * (xpos + 0.5) + a2 * (ypos - 1) * (ypos - 1) - a2 * b2; // 切换到区域2判断值
    while (ypos > 0)                                                               // 区域2：曲线更“平”的部分
    {
        // 四象限对称点
        OLED_SetPixel(x + xpos, y + ypos, color);
        OLED_SetPixel(x - xpos, y + ypos, color);
        OLED_SetPixel(x + xpos, y - ypos, color);
        OLED_SetPixel(x - xpos, y - ypos, color);
        if (d < 0) // d<0：选右下点
        {
            d = d + b2 * ((xpos << 1) + 2) + a2 * (-(ypos << 1) + 3);
            xpos += 1, ypos -= 1;
        }
        else
        {
            // d>=0：选正下点
            d = d + a2 * (-(ypos << 1) + 3);
            ypos -= 1;
        }
    }
}

/**
 * @brief 绘制一张图片
 * @param x 起始点横坐标
 * @param y 起始点纵坐标
 * @param img 图片
 * @param color 颜色
 */
void OLED_DrawImage(uint8_t x, uint8_t y, const Image *img, OLED_ColorMode color)
{
    OLED_SetBlock(x, y, img->data, img->w, img->h, color); // 直接按图片宽高把点阵块拷入显存
}

// ================================ 文字绘制 ================================

/**
 * @brief 绘制一个ASCII字符
 * @param x 起始点横坐标
 * @param y 起始点纵坐标
 * @param ch 字符
 * @param font 字体
 * @param color 颜色
 */
void OLED_PrintASCIIChar(uint8_t x, uint8_t y, char ch, const ASCIIFont *font, OLED_ColorMode color)
{
    /* 字模寻址（ASCIIFont）极简说明：
     * - 字库从 ASCII 空格 ' ' 开始连续存放，所以 (ch - ' ') 就是“第几个字符”的索引
     * - 点阵数据按“列行式/按页存”：竖向每 8 像素占 1 字节，所以高度 h 要先换算成页数 (h+7)/8
     * - 每个字符占用字节数 = ((h+7)/8) * w
     * - font->chars + index * bytes_per_char 就指向该字符点阵的起始地址
     */
    OLED_SetBlock(x, y, font->chars + (ch - ' ') * (((font->h + 7) / 8) * font->w), font->w, font->h, color); // ch-' '：把ASCII映射到字库索引
}

/**
 * @brief 绘制一个ASCII字符串
 * @param x 起始点横坐标
 * @param y 起始点纵坐标
 * @param str 字符串
 * @param font 字体
 * @param color 颜色
 */
void OLED_PrintASCIIString(uint8_t x, uint8_t y, char *str, const ASCIIFont *font, OLED_ColorMode color)
{
    uint8_t x0 = x; // 当前光标 x
    while (*str)    // 指针解引用：*str 为当前字符，遇 '\0' 结束
    {
        OLED_PrintASCIIChar(x0, y, *str, font, color);
        x0 += font->w; // 每画一个字符，x 右移一个字宽
        str++;         // 指针后移到下一个字符
    }
}

/**
 * @brief 获取UTF-8编码的字符长度
 */
uint8_t _OLED_GetUTF8Len(char *string)
{
    if ((string[0] & 0x80) == 0x00) // 0xxxxxxx：单字节 ASCII
    {
        return 1;
    }
    else if ((string[0] & 0xE0) == 0xC0) // 110xxxxx：2字节 UTF-8 头
    {
        return 2;
    }
    else if ((string[0] & 0xF0) == 0xE0) // 1110xxxx：3字节 UTF-8 头
    {
        return 3;
    }
    else if ((string[0] & 0xF8) == 0xF0) // 11110xxx：4字节 UTF-8 头
    {
        return 4;
    }
    return 0; // 非法首字节：返回0给上层做容错处理
}

/**
 * @brief 绘制字符串
 * @param x 起始点横坐标
 * @param y 起始点纵坐标
 * @param str 字符串
 * @param font 字体
 * @param color 颜色
 *
 * @note 为保证字符串中的中文会被自动识别并绘制, 需:
 * 1. 编译器字符集设置为UTF-8
 * 2. 使用波特律动LED取模工具生成字模(https://led.baud-dance.com)
 */
void OLED_PrintString(uint8_t x, uint8_t y, char *str, const Font *font, OLED_ColorMode color)
{
    uint16_t i = 0; // 字节索引（UTF-8 下一个字符可能占多字节，所以 i 不一定每次 +1）

    /* oneLen：字库里“一个字模条目”占用的总字节数
     * - 4 字节：UTF-8 头（用来做匹配）
     * - 其余：点阵数据长度 = ((h+7)/8) * w
     */
    uint8_t oneLen = (((font->h + 7) / 8) * font->w) + 4; // 一个字模占多少字节

    uint8_t found;   // 是否找到字模（0=没找到，1=找到了）
    uint8_t utf8Len; // 当前字符的 UTF-8 编码长度（1/2/3/4）
    uint8_t *head;   // 指向“某个字模条目”的首地址

    while (str[i]) // C字符串以 '\0' 结尾：遇到 0 就退出
    {
        found = 0;                           // 每轮先假设“没找到字模”
        utf8Len = _OLED_GetUTF8Len(str + i); // 读取当前字符的 UTF-8 长度
        if (utf8Len == 0)
            break; // 非法 UTF-8：直接退出（避免死循环）

        // 寻找字符  TODO 优化查找算法, 二分查找或者hash
        for (uint8_t j = 0; j < font->len; j++) // 在字库里线性查找匹配头（字库小的时候足够用）
        {
            head = (uint8_t *)(font->chars) + (j * oneLen); // 定位到第 j 个字模条目首地址
            if (memcmp(str + i, head, utf8Len) == 0)        // memcmp 返回 0 表示“完全相等” -> 命中该字模
            {
                OLED_SetBlock(x, y, head + 4, font->w, font->h, color); // head+4：跳过 UTF-8 头，后面才是点阵
                // 移动光标
                x += font->w; // x 向右移动一个字宽
                i += utf8Len; // i 跳过当前 UTF-8 字符字节数
                found = 1;    // 置位：表示本字符已渲染
                break;
            }
        }

        // 若未找到字模,且为ASCII字符, 则缺省显示ASCII字符
        if (found == 0) // 没找到对应字模：走兜底路径
        {
            if (utf8Len == 1) // 未命中字库但属于 ASCII：按 ASCII 字体兜底显示
            {
                OLED_PrintASCIIChar(x, y, str[i], font->ascii, color); // 直接画原字符
                // 移动光标
                x += font->ascii->w;
                i += utf8Len;
            }
            else
            {
                OLED_PrintASCIIChar(x, y, ' ', font->ascii, color); // 非ASCII未命中时，用空格占位避免错位
                x += font->ascii->w;
                i += utf8Len;
            }
        }
    }
}

void OLED_PrintGlyphByIndex(uint8_t x, uint8_t y, uint8_t glyphIndex, const Font *font, OLED_ColorMode color)
{
    uint8_t oneLen;      // 每个字模条目字节数
    const uint8_t *head; // 指向目标字模条目首地址

    if (font == 0) // 判空保护
    {
        return;
    }
    if (glyphIndex >= font->len) // 越界保护
    {
        return;
    }

    /* 每个字条目：4字节UTF-8头 + 点阵数据 */
    oneLen = (uint8_t)((((font->h + 7) / 8) * font->w) + 4);
    head = (const uint8_t *)font->chars + (uint16_t)glyphIndex * oneLen;

    /* head+4：跳过UTF-8头，直接给点阵数据（列行式） */
    OLED_SetBlock(x, y, head + 4, font->w, font->h, color);
}