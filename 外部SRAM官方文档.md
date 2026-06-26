第 46 章 FSMC-外扩 SRAM 实验

在前面的 FSMC-TFTLCD 实验章节中我们已经介绍过 FSMC，知道了通过它可以外扩存储器。我们使用的 STM32F407ZGT6 本身就有 192K 字节的 SRAM，对一般应用来说，已经足够使用，不过在一些对内存要求高的场合，STM32F4 自带的这些内存就不够用了，比如跑算法或者 GUI 等。因此我们STM32F4 开发板上集成了一颗 1M 字节容量的 SRAM 芯片：IS62WV51216，来满足大内存使用的需求。这一章我们就来学习如何使用 FSMC 控制外扩 1MB 的 SRAM（IS62WV51216） ，实现对 IS62WV51216 的访问控制，并测试其容量。本章要实现的功能是：测试外扩 SRAM（IS62WV51216）的容量，并通过 KEY_UP 和 KEY1 键控制 SRAM 数据的读写 ， 同时控制 DS0 指示灯闪烁 ，提示系统正常运行 。

学习本章可以参考“FSMC-TFTLCD 实验 ”章节内容，IS62WV51216 芯片介绍可以参考“\6--芯片资料\开发板芯片数据手册\IS62WV51216 ”。

本章分为如下几部分内容：
46.1 IS62WV51216 介绍
46.2 FSMC 配置步骤
46.3 硬件设计
46.4 软件设计
46.5 实验现象

46.1 IS62WV51216 介绍

IS62WV51216 是 ISSI（Integrated Silicon Solution, Inc）公司生产的一颗 16 位宽 512K（512*16，即 1M 字节）容量的 CMOS 静态内存芯片。它拥有如下几个特点：
(1) 高速访问。具有 45ns/55ns 访问速度
(2) 低功耗。-36mW（典型）操作功耗。-12uW（典型）待机功耗。
(3) 兼容 TTL 电平接口。
(4) 全静态操作。不需要刷新和时钟电路。
(5) 三态输出。
(6) 字节控制功能。支持高/低字节控制。

A0~18 为地址线，总共 19 根地址线（可访问 2^19=512K 空间（1K=1024））；IO0~15 为数据线，总共 16 根数据线。CS2 和 CS1 都是片选信号，不过 CS2 是高电平有效 CS1 是低电平有效；OE 是输出使能信号（读信号）；WE 为输入使能信号（写信号）；UB 和 LB 分别是高字节控制和低字节控制信号；
我们开发板已将 IS62WV51216 芯片连接在 STM32F4 的 FSMC 上，所以可以直接通过 FSMC 控制。具体连接图在后面硬件设计部分介绍。如果大家想要了解更多 IS62WV51216 芯片信息，可以参考“\6--芯片资料\开发板芯片数据手册\IS62WV51216 ”。
本章，我们使用 FSMC 的 Bank1 区域 3 来控制 IS62WV51216，关于 FSMC的详细介绍，我们在“FSMC-TFTLCD 显示实验 ”已经介绍过，当时我们采用的是读写不同的时序来操作 TFTLCD 模块（因为 TFTLCD 模块读的速度比写的速度慢很多），但是在本章，因为 IS62WV51216 的读写时间基本一致，所以，我们设置读写相同的时序来访问 FSMC。

46.2 FSMC 配置步骤

接下来我们介绍下如何使用库函数对 FSMC 的 Bank1 区域 3 进行配置。这个也是在编写程序中必须要了解的。FSMC 配置步骤在“FSMC-TFTLCD 显示实验 ”章节已经详细讲解，这里我们简单提下，步骤如下：（FSMC 相关库函数在stm32f4xx_ll_fsmc.c、stm32f4xx_hal_sram.c 与其对应头文件中）

（1）使能 FSMC 及端口时钟，并将对应 IO 配置为复用功能要使用 FSMC，首先得开启其时钟。然后需要把 FSMC_D0~15，FSMCA0~18 等相关 IO 口，全部配置为复用输出，并使能各 IO 组的时钟。使能 FSMC 时钟的方法前面 FSMC-TFTLCD 实验已经讲解过，方法为：
__HAL_RCC_FSMC_ CLK_ENABLE();

配置 IO 口为复用输出的关键行代码为：
GPIO_Initure. Mode=GPIO_MODE_AF_PP; / / 推挽 复用
GPIO_Initure. Alternate=GPIO_AF12_FSMC; / / 复用为FSMC

（2）初始化 FSMC，包括 FSMC 区域的选择、读写时间设定等此部分包括设置区域 3 的存储器的工作模式、位宽和读写时序等。本章我们使用模式 A、16 位宽，读写共用一个时序寄存器。这个是通过调用函数HAL_SRAM_Init 来实现的，函数原型为：
HAL_StatusTypeDef HAL_SRAM_Init(SRAM_HandleTypeDef *hsram,FSMC_NORSRAM_TimingTypeDef *Timing, FSMC_NORSRAM_TimingTypeDef*ExtTiming) ;

通过以上几个步骤，我们就完成了 FSMC 的 Bank1 区域 3 的配置，可以访问
IS62WV51216 了，这里还需要注意，因为我们使用的是 Bank1 的区域 3，所以
HADDR[27:26]=10，故外部内存的首地址为 0X68000000。

46.3 硬件设计

本实验使用到硬件资源如下：
（1）DS0 指示灯
（2）KEY_UP 和 KEY1 按键
（3） 串口1
（4）TFTLCD 模块
（5）IS62WV51216
DS0 指示灯、KEY_UP 和 KEY1 按键、串 口 1、TFTLCD 模块电路在前面章节都介绍过，这里就不多说，下面我们看下 IS62WV51216 与 STM32F4 的连接电路图，如下图所示：
从电路图中可以看到，IS62WV51216 与 STM32F1 的连接关系是：
A0-A18 连接在 FSMC_A0-FSMC_A18 上
IO0-IO15 连接在 FSMC_D0-FSMC_D15 上
UB 和 LB 连接在 FSMC_NBL1 和 FSMC_NBL0 上
OE 连接在 FSMC_NOE 上
WE 连接在 FSMC_NWE 上
CE 连接在 FSMC_NE3 上

FSMC 具体对应的 IO 口，大家可以打开开发板原理图查看，这里就不截图。这里提醒下大家：A0-A18 与 FSMC_A0-FSMC_A18 的连接顺序可以打乱，因为地址是固定的，但是 IO0-IO15 和 FSMC_D0-FSMC_D15 的连接顺序不可打乱，否则读写数据将出错。
DS0 指示灯用来提示系统运行状态，KEY_UP 和KEY1按键用来控制IS62WV51216 数据读写，TFTLCD 模块和串口 1 用来显示读写的内容。

46.4 软件设计

本章所要实现的功能是：测试外扩 SRAM（IS62WV51216）的容量，并通过KEY_UP 和KEY1 键控制 SRAM 数据的读写，同时控制DS0 指示灯闪烁，提示系统正常运行。本章实验我们使用的是 FSMC 的 Bank1 区域 3 来控制 IS62WV51216，程序框架如下：
（1）初始化外扩 SRAM（初始化 FSMC 的 Bank1 区域 3）
（2）编写外扩 SRAM 的读写函数
（3）编写外扩 SRAM 容量测试函数
（4）编写主函数
前面介绍 FSMC 配置步骤时，就已经讲解如何初始化 FSMC 的 Bank1 区域 3。下面我们打开“\4--实验程序\7--基础实验--HAL 库版\38-FSMC-外扩 SRAM 实验 ”工程，在 APP 工程组中可以看到添加了 sram.c 文件（里面包含了外扩 SRAM 的驱动程序），在 HAL_Driver 工程组中添加了 stm32f4xx_ll_fsmc.c、stm32f4xx_hal_sram.c 库文件。FSMC 操作的库函数都放在该文件中，所以使用到 FSMC 就必须加入该文件，同时还要包含对应的头文件路径。
