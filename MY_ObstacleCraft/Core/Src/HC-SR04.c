/* ================================================================
 * 文件：HC-SR04.c — 超声波测距模块（非阻塞输入捕获版本）
 *
 * 🎯 功能：测量前方障碍物的距离（cm）
 *
 * ⚙️ 硬件连接：
 *   HC-SR04 有 4 个引脚：
 *     VCC  → 5V（供电）
 *     GND  → GND
 *     TRIG → PA11（GPIO 输出）← 我们发 10us 高电平脉冲
 *     ECHO → PA10（TIM1_CH3）← 回波信号，用定时器测量脉宽
 *
 * 📡 工作原理：
 *   1. 给 TRIG 发 10us 高电平 → 超声波模块发出 8 个 40kHz 脉冲
 *   2. 声波遇到障碍物反射回来 → ECHO 引脚输出高电平
 *   3. ECHO 高电平的持续时间 = 声波往返的时间
 *   4. 距离 = 高电平时间 × 声速(340m/s) ÷ 2（一去一回）
 *      简化：时间(us) × 0.017 = 距离(cm)
 *
 * 🧠 本模块的设计思路（非阻塞）：
 *   传统做法：发 TRIG → while(等待ECHO) → 测量 → 算距离
 *   问题：等待时间不确定（最长等待 38ms 对应 6.5m），CPU 被浪费了。
 *
 *   我们的做法（更高级）：
 *   用 TIM1 的"输入捕获"功能——硬件自动测量脉宽。
 *   CPU 只需发 TRIG，然后继续做别的事（如 OLED 刷新、按键检测）。
 *   当 ECHO 的上升沿/下降沿到来时，TIM1 会产生中断，
 *   我们只在中断里记录时间戳，主循环再来取结果。
 *   这叫"非阻塞"——CPU 利用率更高。
 *
 * 🔧 定时器配置（CubeMX 已配好，不能动）：
 *   TIM1：输入捕获模式，计数频率 = 1MHz（1 个计数 = 1us）
 *         72MHz / (71+1) = 1MHz
 *         CH3 对应 PA10（Echo 引脚）
 *         自动重装载 ARR = 65535（最大测量 65535us ≈ 11m）
 *         已使能 NVIC 中断（TIM1_CC_IRQn）
 *         我们只需要调用 HAL_TIM_IC_Start_IT() 启动捕获即可。
 * ================================================================ */

/*
 * 📚 包含头文件：
 *   HC-SR04.h — 本模块自己的头文件（外部接口声明）
 *   tim.h     — 定时器句柄 htim1 的声明
 */
#include "HC-SR04.h"
#include "tim.h"

/* ================================================================
 * 模块内部静态变量（static = 只有本文件能看到）
 * ================================================================
 *
 * volatile 关键字有三重作用：
 *   1. 告诉编译器：这个变量可能在"中断服务函数"里被修改
 *   2. 阻止编译器优化（比如把变量放到寄存器里而不是内存里）
 *   3. 每次访问都从内存中读取真实值，不使用"缓存"的值
 *
 * 如果不加 volatile，可能出现这种情况：
 *   主循环里 if(echo_new_data) 发现一直是 0，
 *   因为编译器把变量优化到寄存器里了，根本不知道中断已经改了它。
 *   这是一个"经典 bug"——调试器能看到内存变了，但程序就是不知道。
 *   解决方案就是加 volatile！
 */

/* 上升沿到来时 TIM1 的计数值（单位：us） */
static volatile uint32_t echo_start = 0U;

/* 最新一次测量的高电平脉宽（单位：us） */
static volatile uint32_t echo_width_us = 0U;

/* 新数据标志：1=主循环快来读！0=已被读取 */
static volatile uint8_t echo_new_data = 0U;

/* 最新计算出的距离值（cm） */
static volatile int16_t latest_distance_cm = -1;

/* 上次触发 TRIG 的时刻（ms），用于超时判断 */
static uint32_t last_trigger_tick = 0U;

/*
 * 等待上升/下降沿的标志
 *   1 = 正在等待上升沿（ECHO 刚变高，声波还在路上）
 *   0 = 正在等待下降沿（ECHO 刚变低，声波回来了）
 */
static uint8_t wait_rise = 1U;

/*
 * ================================================================
 * 函数：HC_SR04_Delay10us — 10 微秒延时
 * ================================================================
 *
 * 为什么不用 HAL_Delay(1)？
 *   HAL_Delay 的最小单位是 1ms = 1000us，而我们只需要 10us。
 *   而且 HAL_Delay 会阻塞 CPU，违背"非阻塞"的设计理念。
 *
 * 这里用了一个"空循环"来计时。
 *   在 72MHz 下，一个 for 循环+__NOP() 大约需要 55ns，
 *   180 次循环 ≈ 10us。
 *
 * 注意：这是"粗略延时"，不精确。
 *   但对 TRIG 脉冲来说 10~20us 都可以，所以没问题。
 */
static void HC_SR04_Delay10us(void)
{
    /*
     * __NOP() 是"空操作"指令——CPU 什么都不做，纯粹消耗一个时钟周期。
     * volatile 防止编译器把这个循环优化掉（编译器很聪明，会以为"没用"就删了）。
     */
    for (volatile uint32_t i = 0U; i < 180U; ++i)
    {
        __NOP();
    }
}

/*
 * ================================================================
 * 函数：HC_SR04_Init — 模块初始化
 * ================================================================
 *
 * 本函数应该在其他硬件初始化完成后调用（即 MX_TIM1_Init 之后）。
 * 做了这些事：
 *   1. TRIG 引脚输出低电平（初始状态不发射）
 *   2. 所有内部变量清零
 *   3. 启动 TIM1 输入捕获中断
 */
void HC_SR04_Init(void)
{
    /*
     * TRIG 初始化为低电平。
     * 避免上电瞬间误触发超声波（误触发会导致 ECHO 意外中断）。
     * TRIG_GPIO_Port 和 TRIG_Pin 是在 main.h 中定义的宏，
     * 对应 PA11 引脚。
     */
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

    /* 初始化所有静态变量 */
    echo_start = 0U;           // 上升沿计数值清零
    echo_width_us = 0U;        // 脉宽清零
    echo_new_data = 0U;        // 无新数据
    latest_distance_cm = -1;   // 距离无效
    last_trigger_tick = HAL_GetTick();  // 记录当前时间
    wait_rise = 1U;            // 下一次先等上升沿

    /*
     * TIM1 计数器归零。
     * 让计数从 0 开始，避免"历史遗留值"影响首次测量。
     */
    __HAL_TIM_SET_COUNTER(&htim1, 0U);

    /*
     * 设置输入捕获极性为"上升沿触发"。
     * TIM_INPUTCHANNELPOLARITY_RISING = 上升沿
     * TIM_INPUTCHANNELPOLARITY_FALLING = 下降沿
     * 先设为上升沿，因为我们第一步要等 ECHO 变高。
     */
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);

    /*
     * 🚀 启动输入捕获！
     *
     * HAL_TIM_IC_Start_IT — IC = Input Capture（输入捕获）
     *   IT = Interrupt（中断模式，硬件捕获到边沿时产生中断）
     *
     * 调用后，TIM1 开始监听 PA10 上的信号。
     * 当 ECHO 引脚电平变化（上升沿或下降沿），
     * TIM1 会自动记录当前计数器的值到捕获寄存器，
     * 并触发 HAL_TIM_IC_CaptureCallback 中断回调函数。
     */
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_3);
}

/*
 * ================================================================
 * 函数：HC_SR04_Trigger — 发射超声波触发脉冲
 * ================================================================
 *
 * HC-SR04 的触发时序：
 *   1. TRIG 拉高 → 模块开始发射 40kHz 超声波
 *   2. 等待 10us → 模块要求 TRIG 高电平至少保持 10us
 *   3. TRIG 拉低 → 模块开始发送 8 个脉冲
 *   4. 模块自动把 ECHO 拉高，等待回波
 *   5. 收到回波 → ECHO 拉低（ECHO 高电平时间 = 声波飞行时间）
 *
 * 这个函数只做"触发"这件事，不等结果，立刻返回。
 * 测量结果由中断函数记录，app_run 下次循环时来取。
 */
void HC_SR04_Trigger(void)
{
    /* 第1步：TRIG 引脚拉高 */
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);

    /* 第2步：保持高电平 10us */
    HC_SR04_Delay10us();

    /* 第3步：TRIG 引脚拉低（触发完成） */
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

    /*
     * 记录触发时间（用于超时判断）。
     * 如果 200ms 后还没收到回波，说明前方没有反射面（障碍物太远）。
     */
    last_trigger_tick = HAL_GetTick();

    /*
     * 清除"有新数据"标志。
     * 因为这次触发产生的是"新的"测量结果，
     * 上次的结果已经"过期"了。
     */
    echo_new_data = 0U;
}

/*
 * ================================================================
 * 函数：HC_SR04_GetDistance — 获取最新测距结果（非阻塞）
 * ================================================================
 *
 * 📋 返回值含义：
 *   >= 0 : 距离值（单位 cm），比如 50 = 前方 50cm 有障碍物
 *   -1   : 超时（200ms 内没收到回波，障碍物超出量程或没有反射面）
 *   -2   : 测量中（刚触发，还没收到回波，请稍后再查）
 *
 * 这就是"非阻塞"的精髓——调用者不用等，看一眼就知道结果状态。
 * 如果是 -2，下次再来看就行。
 */
int16_t HC_SR04_GetDistance(void)
{
    /* 情况1：有新数据 */
    if (echo_new_data == 1U)
    {
        echo_new_data = 0U;           // 标志清零（"已读取"）
        return latest_distance_cm;     // 返回距离值
    }

    /* 情况2：超时（200ms 没回音） */
    if ((HAL_GetTick() - last_trigger_tick) > 200U)
    {
        /* -1 对上层来说就是"测不到" */
        return -1;
    }

    /* 情况3：还在等待回波 */
    return -2;
}

/*
 * ================================================================
 * 🔥 中断回调函数：HAL_TIM_IC_CaptureCallback
 * ================================================================
 *
 * 这个函数不是我们主动调用的！
 * 它是 HAL 库提供的"弱符号"（weak symbol）函数。
 * 我们在 HC-SR04.c 里重新定义了它，覆盖了 HAL 库的默认版本。
 *
 * 什么时候被调用？
 *   当 TIM1_CH3（PA10）上检测到电平跳变时，
 *   硬件触发中断 → 中断服务程序（stm32f1xx_it.c 里的 TIM1_CC_IRQHandler）
 *   → HAL 库自动调用这个回调函数。
 *
 * 🔑 输入捕获中断处理逻辑：
 *   第1次中断：ECHO 变高了（上升沿）→ 记录当前时间 → 切换极性为下降沿
 *   第2次中断：ECHO 变低了（下降沿）→ 计算脉宽 → 算出距离 → 切换极性为上升沿
 *   这样一次完整的测量就完成了！
 *
 *   示意图：
 *   ECHO引脚  ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
 *             ▏                       ▎
 *             ▏   高电平（声波飞行中） ▎
 *             ▏                       ▎
 *   ──────────▚───────────────────────▚──────
 *             ↑                       ↑
 *         第1次中断(上升沿)        第2次中断(下降沿)
 *         记录 echo_start         计算 echo_width_us
 *
 * 💡 为什么用 static uint8_t wait_rise 来切换？
 *   这是一个"状态机"——两个状态交替进行。
 *   状态A：等上升沿（wait_rise=1）→ 事件：记录时刻，切换到状态B
 *   状态B：等下降沿（wait_rise=0）→ 事件：算距离，切换到状态A
 *
 *   这种设计避免了"连续两次上升沿"或"连续两次下降沿"的错误。
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    /*
     * 判断中断来源：
     *   首先看是哪个定时器（htim->Instance == TIM1）
     *   再看是哪个通道（htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3）
     *   因为可能有多个定时器同时使用输入捕获，不能搞混。
     *   这里用 HAL_TIM_ACTIVE_CHANNEL_3 而不是 TIM_CHANNEL_3，
     *   因为 htim->Channel 存的是"活动通道"的编码格式。
     */
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
    {
        /*
         * HAL_TIM_ReadCapturedValue — 读取捕获寄存器（CCR）
         * 当边沿到来时，TIM1 自动把当前 CNT 寄存器的值存入 CCR。
         * 我们只需要读出来就行，不用自己算时间。
         */
        uint32_t now = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);

        if (wait_rise == 1U)
        {
            /*
             * 🔺 上升沿（ECHO 刚变高）：
             *   - 记录"开始时刻"
             *   - 切换极性为"下降沿"，这样下次中断就是下降沿
             *   - wait_rise=0，下一次进来就走 else 分支
             */
            echo_start = now;  // 记录声波出发时刻

            /*
             * 🎯 __HAL_TIM_SET_CAPTUREPOLARITY — 切换捕获极性
             * 第一次中断是上升沿触发的（我们初始化时设的）。
             * 现在改为下降沿，下次中断就是下降沿了。
             */
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_FALLING);
            wait_rise = 0U;  // 下一次等下降沿
        }
        else
        {
            /*
             * 🔻 下降沿（ECHO 变低了）：
             *   - 脉宽 = now - echo_start（单位：us）
             *   - & 0xFFFF：取低16位（处理计数器溢出回环的情况）
             *     比如 CNT 从 65535 翻转到 0，减法结果就会不对，
             *     但只取低 16 位就自动纠正了。
             *   - 算距离：脉宽(us) × 0.017 = 距离(cm)
             *     推导：声速 340m/s = 0.034cm/us
             *           往返 ÷2 → 0.017cm/us
             *   - 切换极性回"上升沿"，准备下一次测量
             *   - wait_rise=1，下一次等上升沿
             */

            /* 计算脉宽（高电平持续时间），单位：微秒 */
            echo_width_us = (now - echo_start) & 0xFFFFU;

            /*
             * 换算成距离：时间(us) × 声速(cm/us) ÷ 2
             * 340m/s = 34000cm/s = 0.034cm/us
             * ÷2（往返） = 0.017cm/us
             * 所以 1us ≈ 0.017cm
             *
             * 例：echo_width_us = 5000us → 5000 × 0.017 = 85cm
             */
            latest_distance_cm = (int16_t)(echo_width_us * 0.017f);

            /* 标记：新数据就绪！主循环快来取 */
            echo_new_data = 1U;

            /* 恢复极性为上升沿，为下次测量做准备 */
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_3, TIM_INPUTCHANNELPOLARITY_RISING);
            wait_rise = 1U;  // 下一次等上升沿
        }
    }
}

/* ================================================================
 * 兼容旧接口
 * ================================================================
 *
 * 下面两个函数是为了兼容项目中可能存在的旧代码调用习惯。
 * 比如有些教程里函数名叫 HCSR04_Init（没有第二个下划线）。
 *
 * 参数解释：
 *   (void)htim 等的写法——用于"消除编译器未使用参数的警告"。
 *   告诉编译器："我知道这个参数没用到，但我故意不用的，不要报警告。"
 *
 * 这些函数内部直接调用新接口，没有额外逻辑。
 */

/*
 * HCSR04_Init — 旧版初始化接口
 * 参数被保留但不再使用，因为新版本不需要外部传入这些参数。
 * TRIG 引脚宏在 main.h 中已定义，定时器句柄在 tim.h 中已 extern。
 */
void HCSR04_Init(TIM_HandleTypeDef *htim,
                 GPIO_TypeDef *trig_port, uint16_t trig_pin,
                 GPIO_TypeDef *echo_port, uint16_t echo_pin)
{
    (void)htim;         // 不再需要传入定时器句柄
    (void)trig_port;    // TRIG 端口在 main.h 中已有宏定义
    (void)trig_pin;     // TRIG 引脚号也在 main.h 中
    (void)echo_port;    // ECHO 端口已固定在 PA10
    (void)echo_pin;     // ECHO 引脚已固定在 PA10
    HC_SR04_Init();     // 调用新版本的初始化
}

/*
 * HCSR04_GetDistanceCM — 旧版距离获取接口
 * 和新版本的区别：返回值类型为 float，失败时返回 -1.0
 * 新版返回 int16_t，更节省内存（浮点数在单片机里很"贵"）。
 */
float HCSR04_GetDistanceCM(void)
{
    int16_t dist = HC_SR04_GetDistance();
    if (dist < 0)
    {
        return -1.0f;  // 失败返回 -1.0
    }
    return (float)dist;  // int16_t 转 float
}