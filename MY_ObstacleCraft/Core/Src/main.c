/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : 主程序入口 — MY_ObstacleCraft 自动避障小车
 *
 * ─────────────────────────────────────────────────────────────
 * 📌 工程目标：
 *    基于 STM32F103C8 的自动避障小车。
 *    前轮由 DRV8833 驱动直流电机，前部搭载 HC-SR04 超声波传感器，
 *    顶部由 SG-90 舵机控制方向。
 *
 * 🧩 模块分工：
 *    main.c       → 程序入口，硬件初始化，OLED回调，主循环调度
 *    app.c        → 避障状态机（当前处于哪个阶段？该做什么？）
 *    HC-SR04.c    → 超声波测距（非阻塞输入捕获模式）
 *    SG-90.c      → 舵机控制（指数平滑，步态柔和）
 *    DRV8833.c    → 电机驱动（PWM正反转控制）
 *    oled.c       → OLED 屏幕显示
 *    key.c        → 按键事件处理
 *
 * 🔄 主循环节拍：
 *    每 50ms 执行一次 App_Loop() → 测距 → 状态判断 → 控制输出
 *
 * 🔌 引脚分配（关键）：
 *    PA10 → TIM1_CH3 → HC-SR04 Echo（输入捕获）
 *    PA11 → TRIG      → HC-SR04 Trig（GPIO 输出）
 *    PB8  → TIM4_CH3  → SG-90 舵机 PWM
 *    TIM2 → CH1=正转, CH2=反转 → DRV8833 电机
 *    PB12/13/15 → 按键 KEY1/2/3
 *
 * 📐 系统时钟：HSE 8MHz → PLL ×9 → SYSCLK 72MHz
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "app.h"
/* USER CODE END Includes */

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

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/***** 第1部分：用户自定义代码 — CubeMX 不会覆盖的区域 *****/

/*
 * ================================================================
 * 函数：Main_OLEDI2CWrite（OLED I2C 写入回调函数）
 * ================================================================
 *
 * 🔑 什么是"回调函数"？
 *   简单说就是：我们把"怎么用 I2C 发数据"这个具体操作打包成一个函数,
 *   然后把这个函数的地址告诉 oled.c。oled.c 需要发数据的时候就自动调用它。
 *   这样 oled.c 只关心"发什么"，不关心"怎么发"。
 *   以后换 SPI 屏幕？只需要换一个回调函数，oled.c 不用改。
 *   这就是"解耦"——模块之间互相不依赖具体实现。
 *
 * 📋 参数解释：
 *   devAddr   ：I2C 设备地址（0x7A，OLED 的 I2C 地址）
 *   data      ：要发送的数据（比如你要显示 "Hello"，就是 H/e/l/l/o 的 ASCII 码）
 *   len       ：数据长度（几个字节）
 *   timeoutMs ：超时时间，超过这个时间还没发完就算失败
 *   userCtx   ：用户自定义数据，这里传了 I2C 句柄 &hi2c1
 *
 * 🔒 static 关键字：
 *   这个函数前面加了 static，意味着"只有 main.c 内部能看到它"。
 *   其他 .c 文件想调也调不到，防止命名冲突。这叫"文件作用域"。
 *
 * 📦 返回值 HAL_StatusTypeDef：
 *   HAL_OK    → 发送成功
 *   HAL_ERROR → 发送失败
 */
static HAL_StatusTypeDef Main_OLEDI2CWrite(uint16_t devAddr,
                                           const uint8_t *data,
                                           uint16_t len,
                                           uint32_t timeoutMs,
                                           void *userCtx)
{
  /*
   * 🔄 void* 类型转换（Type Casting）：
   *   userCtx 声明为 void*，意思是"我不知道具体是什么类型，先通用地传进来"。
   *   但我们知道调用时传进来的是 I2C_HandleTypeDef 类型的 &hi2c1,
   *   所以用 (I2C_HandleTypeDef*) 把它"强转"回本来面目。
   *
   *   类比：void* 就像"快递盒"，我们拆开后发现里面是手机,
   *   就把手机当手机用，而不是当杯子用。
   */
  I2C_HandleTypeDef *hi2c = (I2C_HandleTypeDef *)userCtx;

  /*
   * 🛡️ 空指针检查（NULL Check）—— 嵌入式必修课
   *   如果 userCtx 传错了（比如传了 NULL），hi2c 就是空指针。
   *   此时如果执行下一行 HAL_I2C_Master_Transmit，CPU 会访问地址 0,
   *   触发硬件错误（HardFault），程序死机重启。
   *
   *   养成习惯：收到指针参数，先判断是不是 NULL。
   *   这叫"防御性编程"——不信任任何外部输入。
   */
  if (hi2c == NULL)
  {
    return HAL_ERROR;
  }

  /*
   * 📤 HAL_I2C_Master_Transmit — HAL 库的 I2C 发送函数
   *
   *   参数逐个看：
   *     hi2c     → &hi2c1（I2C1 外设的句柄，CubeMX 自动生成的全局变量）
   *     devAddr  → 0x7A（OLED 的 I2C 地址）
   *     data     → 要发送的数据（强制去掉 const，因为 HAL 接口没加 const）
   *     len      → 数据长度
   *     timeoutMs→ 20ms 超时
   *
   *   ❓ 为什么 data 是 const 但 HAL 要非 const？
   *     这是 HAL 库设计上的小疏忽——它应该加 const 但没加。
   *     我们作为调用方，强制类型转换一下就行，实际运行没问题。
   */
  return HAL_I2C_Master_Transmit(hi2c, devAddr, (uint8_t *)data, len, timeoutMs);
}
/* USER CODE END 0 */

/**
  * @brief  程序入口点（The application entry point）
  * @note   MCU 上电/复位后，第一条执行的代码就是这里。
  *         就像你在学校里的"开学典礼"——先做准备工作，然后开始上课。
  * @retval int — 嵌入式 main 函数永远不会返回，所以这里的 return 只是形式。
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* 
   * 这里可以放一些"在 HAL 初始化之前就要做的事"。
   * 极少用到，留空即可。
   */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /*
   * 🔧 HAL_Init() — HAL 库的"总开关"
   *   做了三件事：
   *   1. 设置 SysTick 定时器（提供 HAL_GetTick() 的 1ms 时基）
   *   2. 设置 NVIC 优先级分组（决定中断谁的优先级更高）
   *   3. 初始化一些内部变量
   *
   *   ❗特别重要：HAL_GetTick() 从这一刻开始才能正常工作！
   *     在这之前调用 HAL_Delay() 或 HAL_GetTick() 会导致死机。
   */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /*
   * ⏰ SystemClock_Config() — 配置系统时钟
   *   STM32 上电后默认用内部 8MHz 的 HSI 振荡器。
   *   这个函数把它切换到外部 8MHz 晶振（HSE），然后通过 PLL 倍频到 72MHz。
   *   72MHz = 8MHz(HSE) × 9(PLL倍频)
   *   就像给 CPU "超频"——从 8MHz 到 72MHz，快了 9 倍。
   */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /*
   * 🔌 MX_XXX_Init() — 初始化各个外设
   *   这些 MX_ 开头的函数都是 CubeMX 自动生成的。
   *   它们按照 .ioc 文件里的配置，把各个外设的寄存器写对。
   *
   *   执行顺序有讲究：
   *     1. MX_GPIO_Init()      → 先初始化引脚（GPIO），因为别的外设可能依赖引脚模式
   *     2. MX_I2C1_Init()      → I2C 总线（给 OLED 屏幕用）
   *     3. MX_TIM1_Init()      → TIM1：输入捕获模式（给 HC-SR04 超声波测距用）
   *     4. MX_TIM2_Init()      → TIM2：PWM 输出模式（给 DRV8833 电机驱动用）
   *     5. MX_TIM4_Init()      → TIM4：PWM 输出模式（给 SG-90 舵机用）
   *     6. MX_USART2_UART_Init() → 串口（调试用，可以接 USB 转串口看打印信息）
   *
   *   注意：这里只初始化了硬件寄存器，还没启动具体功能。
   *   比如 TIM1 初始化了输入捕获模式，但还没开始真正捕获。
   *   "启动"是在各模块的 Init 函数（如 HC_SR04_Init）里做的。
   */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM4_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  /***** 第2部分：应用层初始化（CubeMX 不会覆盖的区域） *****/

  /*
   * 📺 OLED 初始化三步走：
   *   1. OLED_SetWriteCallback — 告诉 oled.c："你要发数据就调我这个函数"
   *      就像把"快递员的电话"存到通讯录里，需要送货时直接打电话。
   *   2. OLED_SetBusConfig — 设置地址和超时
   *   3. OLED_Init() — 给 OLED 屏幕发送初始化指令序列
   *      告诉它：开始工作、清屏、设置对比度等。
   *
   *   HAL_Delay(10) 是为了等 OLED 内部初始化完成。
   *   就像刚开机时等几秒，让屏幕"反应过来"。
   */
  OLED_SetWriteCallback(Main_OLEDI2CWrite, &hi2c1);
  OLED_SetBusConfig(0x7AU, 20U);
  OLED_Init();
  HAL_Delay(10);

  /*
   * 🚀 App_Init() — 我们自己的应用初始化
   *   在 app.c 中定义，做了这些事：
   *   - 初始化按键（Key_Init）
   *   - 启动超声波传感器（HC_SR04_Init）
   *   - 启动舵机 PWM（SG90_Init）
   *   - 配置电机驱动（DRV8833_Init）
   *   - 设置初始状态为 STATE_IDLE（空闲等待）
   *   - 在 OLED 上显示第一帧画面
   */
  App_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  /*
   * 🔄 主循环（Super Loop）—— 嵌入式程序的"心脏"
   *
   *   没有操作系统（FreeRTOS）的 STM32 程序，全都是这种结构：
   *     while(1) { 做一件事; }
   *
   *   while(1) 的意思是"永远循环，不要停"。
   *   如果跳出这个循环，CPU 就不知道执行什么了——程序就"跑了"。
   *
   *   每次循环：
   *   1. 调用 App_Loop() — 检查传感器、更新状态、控制电机和舵机
   *   2. 回到循环开头，再来一次
   *
   *   App_Loop 内部有 50ms 的节拍控制（通过 HAL_GetTick 判断）,
   *   所以不会疯狂空转，也不会卡死。
   */
  while (1)
  {
    App_Loop();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  /*
   * RCC_OscInitTypeDef 和 RCC_ClkInitTypeDef 是两个结构体,
   * 用来描述"时钟从哪里来"和"时钟要去哪里"。
   * {0} 的意思是"先全部清零"，避免未初始化的成员产生随机值。
   */
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /***** 第一步：配置振荡器（Oscillator）*****/

  /*
   * OscillatorType：我们要配置哪些振荡器？
   *   这里用 HSE（外部高速晶振）和 HSI（内部高速振荡器）。
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;

  /*
   * HSEState：HSE 要不要开启？
   *   RCC_HSE_ON → 开启外部 8MHz 晶振
   */
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;

  /*
   * HSEPredivValue：HSE 预分频值
   *   STM32F103 的 HSE 可以经过一个预分频器再进 PLL。
   *   RCC_HSE_PREDIV_DIV1 = 不分频，8MHz 直接进 PLL。
   */
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;

  /*
   * HSIState：内部 8MHz 振荡器
   *   开启 HSI 作为备用，万一 HSE 失效了还有内部时钟可以用。
   */
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;

  /*
   * PLL（锁相环）配置——相当于"频率倍增器"
   *   PLLState：开启 PLL
   *   PLLSource：PLL 的输入时钟源用 HSE
   *   PLLMUL：倍频系数 ×9 → 8MHz × 9 = 72MHz
   */
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;

  /*
   * HAL_RCC_OscConfig：把上面的配置"写入"寄存器
   *   如果配置不对（比如晶振没焊、频率超出范围），返回 HAL_ERROR。
   *   这时调用 Error_Handler() 死循环，提示"出错了"。
   */
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /***** 第二步：配置时钟总线 *****/

  /*
   * ClockType：我们要配置哪些总线时钟？
   *   HCLK  = AHB 总线时钟（CPU、内存、Flash 都在 AHB 上）
   *   SYSCLK = 系统时钟（CPU 核心时钟）
   *   PCLK1 = APB1 外设总线时钟（低速外设）
   *   PCLK2 = APB2 外设总线时钟（高速外设）
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;

  /*
   * SYSCLKSource：系统时钟来源选 PLL 输出（72MHz）
   */
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;

  /*
   * AHBCLKDivider：AHB 预分频（/1 = 不分频）
   *   AHB 时钟 = SYSCLK / 1 = 72MHz
   */
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;

  /*
   * APB1CLKDivider：APB1 预分频（/2）
   *   APB1 时钟 = AHB时钟 / 2 = 36MHz
   *   APB1 上的外设：I2C1、USART2、TIM2/TIM3/TIM4
   *   注意：TIM2/3/4 在 APB1 上，如果 APB1 分频 ≠ 1,
   *   定时器的时钟会"偷偷"翻倍为 72MHz。
   */
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;

  /*
   * APB2CLKDivider：APB2 预分频（/1 = 不分频）
   *   APB2 时钟 = AHB时钟 / 1 = 72MHz
   *   APB2 上的外设：TIM1、GPIO A~E、USART1、ADC
   */
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  /*
   * HAL_RCC_ClockConfig：应用上面的时钟配置
   *   第二个参数 FLASH_LATENCY_2 是"Flash 等待周期"。
   *   72MHz 时，Flash 的速度跟不上 CPU，所以要插入 2 个等待周期。
   *   就像 CPU 问 Flash"我要数据"，Flash 说"等一下，我在找"，
   *   CPU 就等 2 个时钟周期再拿数据。
   */
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  错误处理函数 — 当 HAL 库检测到异常时调用
  * @note   这里我们做了最"粗暴"的处理：关闭所有中断，然后死循环。
  *         在实际产品中，这里可以记录错误码、闪烁 LED 告警等。
  *
  *         __disable_irq()：关闭全局中断。
  *         为什么这么做？因为出错了就不要再响应任何中断了,
  *         避免在错误状态下"雪上加霜"。
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();  // 关全局中断（CPU 不再响应任何中断请求）
  while (1)         // 死循环，卡在这里不动了
  {
    /*
     * 这里看似什么都没做，实际上 CPU 在疯狂空转。
     * 可以加个 LED 闪烁提示——但本例程没配置错误指示 LED。
     * 可以自己加：HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
     * HAL_Delay(500);  // 记得先开个定时器
     */
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
