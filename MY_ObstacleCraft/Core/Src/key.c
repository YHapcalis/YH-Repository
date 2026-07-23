/*
 * key.c — 输入“采样→事件”转换层（GPIO 按键 + 旋钮统一成 KeyEvent 队列）
 *
 * 在工程网络里：
 * - 上游：App_Loop() 每轮先调用 Key_Task() 刷新输入（见 Core/Src/app.c）
 * - 下游：本文件把识别到的动作写入环形队列；App 与 应用 用 Key_PopEvent() 取走
 * - 旋钮挂载：Key_Task() 末尾调用 Knob_Loop()（Core/Src/kk_knob.c），旋钮转动也会通过 Key_PushEvent() 进入同一个队列
 *
 * 事件约定（见 Core/Inc/key.h）：
 * - KEY_EVENT_DOWN：按下瞬间就推送
 * - KEY_EVENT_SINGLE/DOUBLE：短按先计数，等双击窗口超时后再结算推送
 * - KEY_EVENT_LONG：松开时若按住超过阈值则推送
 *
 * 硬件依赖：
 * - KEYx 引脚宏来自 Core/Inc/main.h（由 gpio.c 初始化为上拉输入）
 * - 时间基准用 HAL_GetTick()（SysTick 中断见 Core/Src/stm32f1xx_it.c）
 */

#include "key.h" // 包含按键模块的头文件：声明/类型/宏定义
// 已禁用#include "knob.h" // 将旋钮作为外设挂载至全局扫描总线

/* 按键判定参数（单位ms） */
#define KEY_DEBOUNCE_MS 20U    // 消抖时间：边沿间隔 < 20ms 认为是抖动；U 表示无符号常量
#define KEY_LONG_PRESS_MS 800U // 长按时间：按住超过 800ms 认为是长按
#define KEY_DOUBLE_GAP_MS 200U // 双击间隔：两次短按之间的时间间隔 < 200ms 认为是双击
#define KEY_EVENT_QSIZE 16U    // 事件队列容量：最多缓存 16 个按键事件
#define KEY_PHYSICAL_COUNT 3U  // 真实GPIO按键数量（KEY1/KEY2/KEY3）

/* 每个按键在运行时要记住的状态 */
typedef struct
{
    GPIO_TypeDef *port; // GPIO 端口指针，例如 GPIOA/GPIOB
    uint16_t pin;       // GPIO 引脚号，例如 GPIO_PIN_12

    uint8_t is_down;          // 当前是否处于“按下”状态：1=按下，0=松开
    uint8_t short_count;      // 短按次数计数：用于单击/双击结算
    uint32_t press_tick;      // 记录按下时刻（HAL_GetTick 的毫秒计数）
    uint32_t last_edge_tick;  // 上次有效边沿时刻：用于消抖
    uint32_t last_short_tick; // 最近一次短按松开时刻：用于双击窗口计时
} KeyState;

static KeyState s_keys[KEY_PHYSICAL_COUNT]; // 按键状态表：仅真实GPIO按键
static KeyEvent s_q[KEY_EVENT_QSIZE];       // 环形事件队列：暂存识别出的 SINGLE/DOUBLE/LONG
static uint8_t s_q_head = 0;                // 队列头指针：写入位置
static uint8_t s_q_tail = 0;                // 队列尾指针：读出位置

void Key_PushEvent(KeyId id, KeyEventType type, uint32_t tick)
{
    uint8_t next = (uint8_t)((s_q_head + 1U) % KEY_EVENT_QSIZE); // 计算环形队列下一个写入位置
    /* 队列满了就丢掉新事件，避免把旧事件顶掉 */
    if (next == s_q_tail) // 若 next 追上 tail，说明队列已满
    {
        return; // 直接丢弃本次事件（防止覆盖旧事件）
    }

    s_q[s_q_head].id = id;     // 写入事件：按键 ID
    s_q[s_q_head].type = type; // 写入事件：事件类型（SINGLE/DOUBLE/LONG）
    s_q[s_q_head].tick = tick; // 写入事件：时间戳（ms）
    s_q_head = next;           // 移动头指针：完成一次入队
}

/*
 * 输入：按键索引 idx。
 * 处理：做消抖、识别按下/松开，并统计短按或触发长按事件。
 * 输出：需要时把事件写入事件队列。
 */
static void Key_ProcessEdgeById(int idx)
{
    KeyState *k = &s_keys[idx];   // 取出该按键的状态结构体指针
    uint32_t now = HAL_GetTick(); // 读取当前系统 tick（单位 ms）

    /* 消抖：边沿间隔太短，认为是抖动，直接忽略 */
    if ((now - k->last_edge_tick) < KEY_DEBOUNCE_MS) // if：判断与上次边沿间隔是否小于消抖时间
    {
        return; // 小于消抖阈值：认为是抖动，忽略本次边沿
    }
    k->last_edge_tick = now; // 记录本次有效边沿时间

    /* 上拉输入：低电平=按下，高电平=松开 */
    if (HAL_GPIO_ReadPin(k->port, k->pin) == GPIO_PIN_RESET) // 读取引脚电平：RESET(0)=按下，SET(1)=松开
    {
        k->is_down = 1U;                                // 标记为“按下”状态
        k->press_tick = now;                            // 记录按下发生的时间
        Key_PushEvent((KeyId)idx, KEY_EVENT_DOWN, now); // [新增] 立刻推入按下事件，无延迟
    }
    else
    {
        if (k->is_down) // 必须先按下过，再读到松开，才算一次完整点击
        {
            uint32_t press_ms = now - k->press_tick; // 计算按住时长 = 当前时间 - 按下时间
            k->is_down = 0U;                         // 更新状态：现在已经松开

            /* 按住时间超过阈值 -> 长按事件 */
            if (press_ms >= KEY_LONG_PRESS_MS) // if：若按住时间达到长按阈值
            {
                Key_PushEvent((KeyId)idx, KEY_EVENT_LONG, now); // 入队一个“长按”事件
                k->short_count = 0U;                            // 长按后清空短按计数，避免与单击/双击混淆
            }
            else
            {
                /* 短按先计数，稍后再判断是单击还是双击 */
                if (k->short_count < 2U) // 限制最大计数为 2：只关心单击/双击
                {
                    k->short_count++; // 自增：记录一次短按
                }
                k->last_short_tick = now; // 记录最近一次短按松开时间：用于双击窗口判断
            }
        }
    }
}

/*
 * 按键初始化函数，配置按键对应的 GPIO 端口/引脚，并初始化状态变量。
 */
void Key_Init(void)
{
    /*
    KnobConfig knobConfig;        // 定义一个局部配置结构体：用于给旋钮模块传参数
    knobConfig.htim = &htim1;     // 把 TIM1 句柄地址传给旋钮模块（取地址符 &）
    knobConfig.debounceMs = 100U; // 设置旋钮防抖时间为 100ms（U=无符号常量）
    Knob_SetConfig(&knobConfig);  // 调用配置接口，把参数写入旋钮模块内部
    Knob_Init();                  // 初始化旋钮硬件状态（计数器基准值等）
    */
    /* 事件队列复位（只在上电初始化调用时生效） */
    s_q_head = 0U; // 头指针归零：从队列起点开始写入
    s_q_tail = 0U; // 尾指针归零：从队列起点开始读出

    /* 这里是板级配置点：换板子时一般只改这里的 KEYx 引脚映射 */

    /* 按键ID与实际端口/引脚的绑定 */
    s_keys[KEY_ID_1].port = KEY1_GPIO_Port; // 绑定 KEY1 的 GPIO 端口
    s_keys[KEY_ID_1].pin = KEY1_Pin;        // 绑定 KEY1 的 GPIO 引脚

    s_keys[KEY_ID_2].port = KEY2_GPIO_Port; // 绑定 KEY2 的 GPIO 端口
    s_keys[KEY_ID_2].pin = KEY2_Pin;        // 绑定 KEY2 的 GPIO 引脚

    s_keys[KEY_ID_3].port = KEY3_GPIO_Port; // 绑定 KEY3 的 GPIO 端口
    s_keys[KEY_ID_3].pin = KEY3_Pin;        // 绑定 KEY3 的 GPIO 引脚

    /* 初始化运行状态，避免上电瞬间误判 */
    uint32_t now = HAL_GetTick(); // 读取当前时间：用于初始化消抖时间戳
    for (uint8_t i = 0; i < KEY_PHYSICAL_COUNT; i++)
    {
        s_keys[i].is_down = 0U;                           // 初始认为未按下
        s_keys[i].press_tick = 0U;                        // 按下时间清零
        s_keys[i].last_edge_tick = now - KEY_DEBOUNCE_MS; // 让首次边沿不会被消抖直接过滤
        s_keys[i].short_count = 0U;                       // 短按计数清零
        s_keys[i].last_short_tick = 0U;                   // 最近短按时间清零
    }
}
/*
 * 按键扫描任务函数，供主循环调用。每次调用都轮询扫描按键状态，识别边沿并处理事件。
 */
void Key_Task(void)
{
    uint32_t now = HAL_GetTick();

    /*
     * 轮询扫描思路：
     * - 每次进来都读取按键引脚电平
     * - 只有当“当前电平对应的按下/松开状态”和上次记录的 is_down 不一致时，
     *   才认为可能发生了边沿（按下或松开），交给 Key_ProcessEdgeById() 做消抖和事件识别
     *
     * 注意：不能每轮都硬调 Key_ProcessEdgeById()
     * 否则按住不放时 press_tick 会反复刷新，长按时间会算错。
     */
    for (uint8_t i = 0; i < KEY_PHYSICAL_COUNT; i++)
    {
        KeyState *k = &s_keys[i]; // 取当前按键状态结构体地址，便于后续访问成员

        /* 上拉输入：低电平=按下，高电平=松开 */
        uint8_t down_now = (HAL_GPIO_ReadPin(k->port, k->pin) == GPIO_PIN_RESET) ? 1U : 0U;

        if (down_now != k->is_down) // 电平状态发生变化：可能是按下或松开边沿
        {
            Key_ProcessEdgeById(i); // 交给边沿处理函数（内含消抖/长按/短按统计）
        }
    }

    /* 超过双击等待窗口后，结算短按为单击或双击 */
    for (uint8_t i = 0; i < KEY_PHYSICAL_COUNT; i++)
    {
        KeyState *k = &s_keys[i]; // 再次取当前按键状态，用于双击窗口结算

        if (k->short_count > 0U && (uint32_t)(now - k->last_short_tick) > KEY_DOUBLE_GAP_MS)
        {
            if (k->short_count == 1U) // if 分支：只有 1 次短按 -> 单击
            {
                Key_PushEvent((KeyId)i, KEY_EVENT_SINGLE, now); // 推入单击事件
            }
            else
            {
                Key_PushEvent((KeyId)i, KEY_EVENT_DOUBLE, now); // 推入双击事件
            }
            k->short_count = 0U; // 结算完成后清零计数，等待下一轮短按
        }
    }

    // 【新增】顺带扫描旋钮：调用 Key_Task 时，按键和旋钮都一起更新
    // 已禁用 Knob_Loop();
}

/*
 * 事件出队函数：从事件队列中取出一个事件，存入 evt 指向的结构体中。
 */
uint8_t Key_PopEvent(KeyEvent *evt)
{
    if (evt == NULL) // 参数判空：防止空指针解引用导致异常
    {
        return 0U; // 参数检查：如果 evt 是 NULL，直接返回 0 表示没有事件
    }

    if (s_q_tail == s_q_head) // 队列空了：tail 追上 head，说明没有事件可读
    {
        return 0U; // 返回 0 表示没有事件
    }

    *evt = s_q[s_q_tail];                                    // 把队尾事件拷贝到调用者提供的结构体
    s_q_tail = (uint8_t)((s_q_tail + 1U) % KEY_EVENT_QSIZE); // 尾指针 +1（取模实现环形回绕）

    return 1U; // 返回 1 表示成功取出事件
}
