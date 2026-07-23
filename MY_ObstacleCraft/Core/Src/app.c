/* ================================================================
 * 文件：app.c — 应用程序层（自动避障状态机）
 *
 * 📌 这个文件是"避障小车的脑子"：
 *   它负责决定——当前应该前进/后退/转弯/停止。
 *
 * 🔄 工作流程（每 50ms 循环一次）：
 *   1. 读取超声波距离
 *   2. 触发下一次测距
 *   3. 滤波去噪
 *   4. 检查按键输入
 *   5. 状态机迁移（根据距离判断要不要改变行为）
 *   6. 执行控制（设置电机速度、舵机角度）
 *   7. 更新 OLED 显示
 *
 * 🎯 设计模式：状态模式（State Pattern）
 *   当前处于哪个状态（IDLE/FORWARD/CAUTION/AVOID/RECOVERY/FAULT），
 *   就执行对应的行为，并在满足条件时切换到其他状态。
 * ================================================================ */

/*
 * 📚 头文件包含：
 *   每个头文件就像一本"说明书"——告诉编译器有哪些函数和变量可以用。
 */
#include "app.h"         // 本模块的头文件（声明了全局变量和函数原型）
#include "oled.h"        // OLED 屏幕显示功能
#include "key.h"         // 按键检测（KEY1 启动避障）
#include "HC-SR04.h"     // 超声波测距（测量前方障碍物距离）
#include "SG-90.h"       // 舵机控制（转向）
#include "DRV8833.h"     // 电机驱动（前进/后退）
#include "usart.h"       // 串口调试输出
#include "stdio.h"       // C 标准库——sprintf 函数（格式化字符串）
#include "string.h"      // C 标准库——字符串操作
#include "tim.h"         // 定时器句柄声明（htim1, htim2, htim4）

/* ================================================================
 * 第1部分：避障参数（可以按实际情况调整）
 * ================================================================
 *
 * 这些 #define 相当于"常量"——编译的时候直接替换成数字。
 * 好处是：想改参数只需要改这一个地方，不用满文件搜数字。
 *
 * 距离单位：厘米（cm）
 * ──────────────────────────────────────────
 * 0cm    40cm    80cm    120cm           ∞
 * ├─危险─┼─警惕─┼─安全──┼─非常安全─────┤
 *  AVOID  CAUTION  FORWARD
 */

/* 安全距离：超过这个距离，放心往前走 */
#define SAFE_DIST_CM       120

/* 警惕距离：小于这个距离，减速并开始打方向试探 */
#define CAUTION_DIST_CM    80

/* 避障距离：小于这个距离，立即后退+急转弯 */
#define AVOID_DIST_CM      40

/* 各状态下的电机 PWM 占空比（0~100，越大越用力） */
#define NORMAL_PWM         70   // 正常前进速度
#define CAUTION_PWM        55   // 减速警惕速度
#define REVERSE_PWM        65   // 倒车速度

/* 舵机最大偏转角（度数），正数 = 右转，负数 = 左转 */
#define MAX_SG90_ANGLE     35

/* ================================================================
 * 第2部分：全局变量（整个工程都能访问）
 * ================================================================
 *
 * "全局"的意思是：其他 .c 文件通过 "extern" 声明后也能读写这些变量。
 * 比如 oled.c 在读 g_distance_cm 来显示距离数值。
 * 我们在 app.h 里用 "extern" 声明了它们。
 *
 * 初始值说明：
 *   g_distance_cm = -1   → "还没测到距离"（传感器刚开始工作时可能没数据）
 *   g_app_state = STATE_IDLE → 上电后先停在"空闲"状态，等按键启动
 */

int16_t  g_distance_cm   = -1;   // 最新滤波后的距离值（cm），负值表示无效
uint8_t  g_DRV8833_pwm   = 0;    // 电机当前 PWM 占空比（0~100）
int8_t   g_DRV8833_dir   = 0;    // 电机方向：1=前进，-1=后退，0=停止
int8_t   g_SG90_angle    = 0;    // 舵机当前偏转角（-35~+35，正=右）
AppState g_app_state     = STATE_IDLE;  // 当前状态机状态

/* ================================================================
 * 第3部分：模块内部变量（"static" = 只有本文件能看到）
 * ================================================================
 *
 * static 变量就像"藏在文件内部的秘密"——其他 .c 文件访问不到。
 * 这样做好处多：
 *   1. 防止外部意外修改
 *   2. 命名不用怕冲突（别的文件可以定义同名变量）
 *   3. 变量作用域清晰，容易维护
 */

/* 上次执行 app_run() 的时刻（ms），用于控制 50ms 执行间隔 */
static uint32_t last_run_tick     = 0;

/* 上次刷新 OLED 的时刻（ms），用于控制 500ms 刷新间隔 */
static uint32_t last_display_tick = 0;

/* ================================================================
 * 中值滤波（Median Filter）—— 对抗"异常跳变"
 * ================================================================
 *
 * 超声波传感器容易受干扰，偶尔会读到"离谱"的数值。
 * 比如明明前面 50cm，突然跳到 300cm 然后又跳回来。
 *
 * 中值滤波的原理：
 *   连续取 3 次测量值，排序后取中间的那个。
 *   比如读到 {48, 52, 300} → 排序后 {48, 52, 300} → 取 52。
 *   这样 300 这个"异常值"就被过滤掉了。
 *
 * 为什么不用平均值？
 *   平均值 {48+52+300}/3 ≈ 133——原本的 50cm 被 300 拉到了 133，不准了。
 *   中值滤波对"偶尔的毛刺"效果更好。
 */

 /* 环形缓冲——最近 3 次有效测量值 */
static int16_t filter_buf[3] = {SAFE_DIST_CM, SAFE_DIST_CM, SAFE_DIST_CM};

/* 环形缓冲的写入位置（0→1→2→0→1→2→...一直循环） */
static uint8_t filter_idx = 0;

/* 连续无效数据的计数器 */
static uint8_t invalid_count = 0;

static int16_t median_filter(int16_t raw)
{
    /*
     * 情况A：无效数据（raw < 0）
     *   HC_SR04_GetDistance() 返回 -1（超时）或 -2（测量中）
     *   我们不信任单次无效值，但连续 5 次无效说明真的出问题了。
     */
    if (raw < 0)
    {
        invalid_count++;  // 记录一次失败

        /*
         * 连续 5 次都是无效数据 → 传感器可能故障或接线松了。
         * 保守策略：把缓冲区的 3 个值全部重置为"安全距离"，
         * 避免小车因为历史数据而误判。
         */
        if (invalid_count >= 5)
        {
            filter_buf[0] = filter_buf[1] = filter_buf[2] = SAFE_DIST_CM;
            return SAFE_DIST_CM;  // 返回安全距离，让小车"谨慎前进"
        }

        /*
         * 偶尔一次无效：返回上一次的有效值（"保持"策略）
         * (filter_idx + 2) % 3 是取"最早存的那个值"
         * 因为 filter_idx 指向"下一次要写的位置"，所以 (idx+2)%3 是上次的
         */
        return filter_buf[(filter_idx + 2) % 3];
    }

    /*
     * 情况B：有效数据（raw >= 0）
     * 既然来了有效数据，故障计数器清零。
     */
    invalid_count = 0;

    /*
     * 把新数据写入环形缓冲区当前位置，然后指针移到下一个位置。
     * filter_idx 一直在 0→1→2→0→1→2→... 循环。
     * 所以 buffer 里永远是最新的 3 次测量值。
     */
    filter_buf[filter_idx] = raw;
    filter_idx = (filter_idx + 1) % 3;

    /*
     * 3 个数取中位数——"排序法"：
     * 把 a, b, c 从小到大排列，取中间那个。
     * 使用了"交换"方法：如果 a > b，就交换它们的值。
     *
     * 示例：
     *   原始: a=52, b=48, c=300
     *   1. a>b? 52>48 → 交换 → a=48, b=52, c=300
     *   2. b>c? 52>300 → 不交换
     *   3. a>b? 48>52 → 不交换
     *   结果: a=48, b=52(中位数), c=300
     */
    int16_t a = filter_buf[0];
    int16_t b = filter_buf[1];
    int16_t c = filter_buf[2];
    int16_t tmp;  // 临时变量，用于交换值

    // 第1轮：确保 a <= b
    if (a > b) { tmp = a; a = b; b = tmp; }
    // 第2轮：确保 b <= c（现在 a 已经 ≤ b）
    if (b > c) { tmp = b; b = c; c = tmp; }
    // 第3轮：再次确保 a <= b（因为第2轮可能改变了 b）
    if (a > b) { tmp = a; a = b; b = tmp; }

    return b;  // b 就是三个数中的中位数
}

/* ================================================================
 * 防抖计数器（Debounce Counters）
 * ================================================================
 *
 * 为什么需要"防抖"？因为超声波测距是有噪声的。
 * 可能某次测到 38cm（<40cm），但下一次又是 45cm。
 * 如果直接切换状态，小车就会在 FORWARD 和 AVOID 之间"疯狂摇摆"。
 *
 * 解决方案：连续 N 次满足条件才切换状态。
 * 比如 "连续 3 次 <80cm" 才进入 CAUTION 状态。
 * 这就像"重要的事情说三遍"——三次确认再行动。
 */

/* 各防抖计数器：记录当前条件连续满足了多少次 */
static uint8_t caution_cnt  = 0;  // 进入警惕状态的条件计数
static uint8_t avoid_cnt    = 0;  // 进入避障状态的条件计数
static uint8_t forward_cnt  = 0;  // 恢复前进状态的条件计数
static uint8_t recovery_cnt = 0;  // 从避障恢复的条件计数
static uint8_t fault_cnt    = 0;  // 进入/退出故障状态的条件计数

/*
 * reset_debounce — 清零所有防抖计数器
 * 每次状态切换时都要调用，因为新的状态应该重新计数。
 */
static void reset_debounce(void)
{
    caution_cnt  = 0;  // CAUTION 条件计数清零
    avoid_cnt    = 0;  // AVOID 条件计数清零
    forward_cnt  = 0;  // FORWARD 条件计数清零
    recovery_cnt = 0;  // RECOVERY 条件计数清零
    fault_cnt    = 0;  // FAULT 条件计数清零
}

/* ================================================================
 * OLED 显示辅助函数
 * ================================================================
 */

/*
 * state_name — 把状态枚举转成可读的字符串
 * 比如 STATE_FORWARD → "FORWARD"
 * 这样在 OLED 上显示的就是文字，而不是数字 1。
 */
static const char *state_name(AppState s)
{
    switch (s)
    {
        case STATE_IDLE:     return "IDLE   ";  // 空闲
        case STATE_FORWARD:  return "FORWARD";  // 前进
        case STATE_CAUTION:  return "CAUTION";  // 警惕
        case STATE_AVOID:    return "AVOID  ";  // 避障
        case STATE_RECOVERY: return "RECOVER";  // 恢复
        case STATE_FAULT:    return "FAULT  ";  // 故障
        default:             return "UNKNOWN";  // 未知（不会发生）
    }
}

/*
 * update_oled — 刷新 OLED 显示内容
 *
 * OLED 屏幕分辨率：128×64 像素。
 * 这里用了 12×6 像素的 ASCII 字体，每行高度 14 像素。
 *
 * 显示布局（共 4 行）：
 *   行0: St:FORWARD        ← 当前状态
 *   行1: Dist: 120cm       ← 测距结果
 *   行2: Mtr:FWD  70       ← 电机方向+速度
 *   行3: Srv:+35deg        ← 舵机角度
 */
static void update_oled(void)
{
    char line[22];  // 22 字节的临时缓冲区，用于格式化文本

    // 第1行：状态（State）
    OLED_PrintASCIIString(0, 0,  (char*)"St:",    &afont12x6, OLED_COLOR_NORMAL);
    OLED_PrintASCIIString(24, 0, (char*)state_name(g_app_state), &afont12x6, OLED_COLOR_NORMAL);

    // 第2行：距离（Distance）
    if (g_distance_cm >= 0)
        sprintf(line, "Dist:%3dcm", g_distance_cm);  // 如 "Dist:120cm"
    else
        sprintf(line, "Dist: N/A ");                  // 无效数据
    OLED_PrintASCIIString(0, 14, line, &afont12x6, OLED_COLOR_NORMAL);

    // 第3行：电机（Motor）
    const char *dir_str;  // 方向字符串
    if      (g_DRV8833_dir == 1)  dir_str = "FWD";   // 前进
    else if (g_DRV8833_dir == -1) dir_str = "REV";   // 后退
    else                           dir_str = "STOP";  // 停止
    sprintf(line, "Mtr:%s %3d", dir_str, g_DRV8833_pwm);  // 如 "Mtr:FWD  70"
    OLED_PrintASCIIString(0, 28, line, &afont12x6, OLED_COLOR_NORMAL);

    // 第4行：舵机（Servo）
    sprintf(line, "Srv:%+3ddeg", g_SG90_angle);  // 如 "Srv:+35deg"
    OLED_PrintASCIIString(0, 42, line, &afont12x6, OLED_COLOR_NORMAL);

    /*
     * OLED_ShowFrame() — 把显存中的内容真正发送到屏幕。
     * 之前调用的 OLED_PrintASCIIString 只是修改了内存中的"显存"，
     * 要调用 ShowFrame 才会把数据通过 I2C 发给 OLED 屏幕。
     * 这叫"双缓冲"（Double Buffering）——先画好，再一次性显示。
     */
    OLED_ShowFrame();
}

/* ================================================================
 * 第4部分：公开 API — 应用初始化
 * ================================================================
 *
 * app_init() 在 main.c 的 MX_XXX_Init 之后、while(1) 之前调用。
 * 负责初始化所有模块，让系统进入待命状态。
 */
void app_init(void)
{
    /* 1. 初始化按键模块 */
    Key_Init();

    /* 2. 初始化超声波传感器（启动 TIM1 输入捕获） */
    HC_SR04_Init();

    /* 3. 初始化舵机（启动 TIM4 PWM 输出，舵机回中 90°） */
    SG90_Init();

    /*
     * 4. 初始化电机驱动
     *    TIM2_CH1 → 正转（前进）
     *    TIM2_CH2 → 反转（后退）
     *    初始化后先把电机停在安全状态。
     */
    DRV8833_Init(&htim2, TIM_CHANNEL_1, TIM_CHANNEL_2);
    DRV8833_Stop();     // 确保电机不转
    g_DRV8833_dir = 0;  // 方向=停止
    g_DRV8833_pwm = 0;  // 速度=0

    /* 5. 舵机回中（指向正前方） */
    g_SG90_angle = 0;
    SG90_SetAngle(0);

    /* 6. 滤波缓冲区初始化 */
    for (uint8_t i = 0; i < 3; i++)
        filter_buf[i] = SAFE_DIST_CM;  // 全部填充为安全距离
    filter_idx    = 0;  // 写入位置从头开始
    invalid_count = 0;  // 无效计数清零

    /* 7. 状态机初始状态 */
    reset_debounce();     // 防抖计数器清零
    g_distance_cm = -1;   // 距离初始化为"无效"
    g_app_state   = STATE_IDLE;  // 从空闲状态开始

    /* 8. 记录当前时刻，用于后续的时间判断 */
    last_run_tick     = HAL_GetTick();  // 记录主循环开始时间
    last_display_tick = HAL_GetTick();  // 记录 OLED 开始时间

    /* 9. 首次刷新 OLED 画面 */
    OLED_NewFrame();   // 清空显存
    update_oled();     // 绘制并显示
}

/*
 * ================================================================
 * 函数：app_run — 应用主循环（每 50ms 执行一次）
 * ================================================================
 *
 * 这是整个避障小车的"心脏"——所有决策都在这里完成。
 * 每次 main.c 的 while(1) 循环都会调用这个函数。
 *
 * 📋 执行步骤：
 *   Step 1 : 读取上次触发的超声波测距结果
 *   Step 2 : 触发下一次超声波测距（非阻塞，立刻返回）
 *   Step 3 : 中值滤波，去除噪声
 *   Step 4 : 处理按键事件（KEY1=启动）
 *   Step 5 : 状态机迁移（根据距离判断是否切换状态）
 *   Step 6 : 舵机平滑更新
 *   Step 7 : 每 500ms 刷新一次 OLED 显示
 *
 * ⏱ 时间控制：
 *   使用 HAL_GetTick()（系统上电后的毫秒数）来控制执行频率。
 *   如果距离上次执行不到 50ms，直接返回，不做事。
 *   这叫"非阻塞延时"——CPU 不会卡住等，而是"到时间了再做"。
 */
void app_run(void)
{
    /* ---- 时间检查：是否到了该执行的时候？ ---- */
    uint32_t now = HAL_GetTick();  // 获取当前时间（ms）
    /*
     * 如果现在距离上次执行还没到 50ms，跳过本次循环。
     * 好处是：主循环跑得快（每次只需简单比较一下）,
     *         但业务逻辑按固定 50ms 节拍执行。
     * 这也意味着超声波最短测量间隔 = 50ms。
     */
    if (now - last_run_tick < 50) return;
    last_run_tick = now;  // 更新"最后执行时间"

    /*
     * ⚠️ 关键顺序提醒：
     *   必须先 GetDistance 再 Trigger！
     *   因为 HC_SR04_Trigger() 会清除 echo_new_data 标志,
     *   如果先 Trigger，上次的测量结果就被丢掉了。
     */

    /* ---- Step 1: 读取距离（读的是"上次触发"的结果） ---- */
    /*
     * HC_SR04_GetDistance() 可能返回：
     *   >= 0 : 有效距离值（cm）
     *   -1   : 超时（200ms 内没收到回波 → 障碍物太远或没反射面）
     *   -2   : 测量中（触发后还在等回波）
     */
    int16_t raw = HC_SR04_GetDistance();

    /* ---- Step 2: 触发下一次测距（非阻塞） ---- */
    /*
     * HC_SR04_Trigger() 会发 10us 的 Trig 脉冲,
     * 然后立刻返回——不等结果。
     * 结果会在下次 app_run() 时通过 GetDistance 读取。
     * 这就是"非阻塞"——不浪费 CPU 时间等待回波。
     */
    HC_SR04_Trigger();

    /* ---- Step 3: 中值滤波 ---- */
    int16_t dist = median_filter(raw);  // 去噪声
    g_distance_cm = dist;               // 更新全局距离变量

    /* ---- Step 4: 按键处理 ---- */
    /*
     * Key_Task()：刷新按键状态（检测电平变化、计时防抖）。
     * 每次循环都要调用，否则按键检测会"卡住"。
     *
     * Key_PopEvent()：从事件队列取出一个按键事件。
     * 返回 1 = 有事件，0 = 队列空。
     * 用 while 循环把队列里的所有事件都处理完。
     */
    Key_Task();
    KeyEvent evt;  // 按键事件结构体
    while (Key_PopEvent(&evt))
    {
        /*
         * 判断是否是"KEY1 短按"。
         * KEY_ID_1 = PB12 上的按键。
         * 只有在 IDLE（空闲）状态时按下才有效——防止误操作。
         */
        if (evt.id == KEY_ID_1 && evt.type == KEY_EVENT_SINGLE)
        {
            /*
             * 如果当前是空闲状态，KEY1 按下 → 启动避障
             * 状态切换到 FORWARD（前进），防抖计数器清零。
             */
            if (g_app_state == STATE_IDLE)
            {
                g_app_state = STATE_FORWARD;  // 启动！
                reset_debounce();              // 全新开始，计数清零
            }
        }
    }

    /* ============================================================
     * Step 5: 状态机（State Machine）—— 小车决策的核心
     * ============================================================
     *
     * 这是一个 switch-case 结构，根据当前状态执行不同的逻辑。
     * 每个 case 里：
     *   1. 判断条件是否满足（距离对比 + 防抖计数）
     *   2. 如果满足 → 切换状态（并重置防抖计数器）
     *   3. 如果不满足 → 继续当前状态的行为
     *
     * 🎯 状态迁移图：
     *
     *     ┌──────────────────────────────────────────────┐
     *     │                  IDLE                         │
     *     │            (等待 KEY1 按键启动)                │
     *     └────────┬─────────────────────────────────────┘
     *              │ KEY1 短按
     *              ▼
     *     ┌──────────────────────────────────────────────┐
     *     │               FORWARD                         │
     *     │         (全速前进，舵机朝前)                    │
     *     └──┬───────────────┬───────────────────────────┘
     *        │               │
     *   dist<80×3      dist<40×2
     *        │               │
     *        ▼               ▼
     *  ┌──────────┐   ┌──────────┐
     *  │ CAUTION  │   │  AVOID   │
     *  │(减速+试探)│   │(倒车+转向)│
     *  └──┬───┬───┘   └────┬─────┘
     *     │   │            │ dist>80×5
     *     │   │            ▼
     *     │   │      ┌──────────┐
     *     │   └──────│ RECOVERY │
     *     │   dist<40×2 │(前进+回中)│
     *     │          └────┬─────┘
     *     │               │ dist>100+回中×3
     *     │               │
     *     └── dist>120×3──┘
     *
     *   任意运行态 ── 连续 5 次超时 ──▶ FAULT ── 连续 3 次有效 ──▶ IDLE
     */
    switch (g_app_state)
    {
        /* ---- 状态：空闲（IDLE） ---- */
        case STATE_IDLE:
            /*
             * 空闲状态下的行为：
             * - 电机停止
             * - 舵机朝前
             * - 等待 KEY1 按键启动
             *
             * 每次循环都重新设置一次，确保安全。
             * 即使之前有人手动转动了电机/舵机，也会被纠正回来。
             */
            DRV8833_Stop();        // 电机停转
            g_DRV8833_dir = 0;     // 方向=停止
            g_DRV8833_pwm = 0;     // 速度=0
            g_SG90_angle  = 0;     // 舵机角度=0（朝前）
            SG90_SetAngle(0);      // 通知舵机模块
            break;  // break 跳出 switch，不继续往下执行

        /* ---- 状态：前进（FORWARD） ---- */
        case STATE_FORWARD:
            /*
             * 正常前进时，持续监测距离。
             *
             * 子条件1：距离 < 40cm（危险！）
             *   连续 2 次 → 立即进入 AVOID（避障）状态
             *
             * 子条件2：40cm ≤ 距离 < 80cm（有障碍靠近）
             *   连续 3 次 → 进入 CAUTION（警惕）状态
             *
             * 子条件3：距离 ≥ 80cm（安全）
             *   保持前进，防抖计数器清零
             */

            /* 危险！障碍物太近了 */
            if (dist >= 0 && dist < AVOID_DIST_CM)
            {
                avoid_cnt++;        // 避障条件+1
                caution_cnt = 0;    // 警惕条件清零（不叠加）
                if (avoid_cnt >= 2) // 连续2次都这么近 → 确认危险
                {
                    g_app_state = STATE_AVOID;  // 切换到避障
                    reset_debounce();            // 新状态，重新计数
                    break;  // 跳出 switch，不再执行后面的 FORWARD 代码
                }
            }
            /* 注意：障碍物在靠近 */
            else if (dist >= 0 && dist < CAUTION_DIST_CM)
            {
                caution_cnt++;      // 警惕条件+1
                avoid_cnt = 0;      // 避障条件清零
                if (caution_cnt >= 3) // 连续3次都偏近 → 确认需要减速
                {
                    g_app_state = STATE_CAUTION;  // 切换到警惕
                    reset_debounce();
                    break;
                }
            }
            /* 安全距离，一切正常 */
            else
            {
                caution_cnt = 0;  // 安全了，清零警惕计数
                avoid_cnt   = 0;  // 清零避障计数
            }

            /*
             * 执行前进动作：
             * - 电机正转，速度 NORMAL_PWM（70%）
             * - 舵机朝前（0°）
             * 如果上面条件满足并切换了状态，这里的代码不会执行
             * （因为前面已经 break 了）。
             */
            DRV8833_Forward(NORMAL_PWM);  // 全速前进
            g_DRV8833_dir = 1;            // 方向=前进
            g_DRV8833_pwm = NORMAL_PWM;   // 速度=70
            g_SG90_angle  = 0;            // 舵机朝前
            SG90_SetAngle(0);
            break;

        /* ---- 状态：警惕（CAUTION） ---- */
        case STATE_CAUTION:
            /*
             * 警惕状态：前方有障碍但还不太近。
             * 行为：减速前进 + 根据距离打方向（试探性绕行）
             *
             * 子条件1：距离 < 40cm（越来越近了！）
             *   连续 2 次 → 升级为 AVOID
             *
             * 子条件2：距离 ≥ 120cm（障碍过去了/远离了）
             *   连续 3 次 → 恢复 FORWARD
             *
             * 子条件3：40cm ≤ 距离 < 120cm（保持警惕）
             *   保持 CAUTION，根据距离调整偏转方向
             */

            /* 再次恶化 → 避障 */
            if (dist >= 0 && dist < AVOID_DIST_CM)
            {
                avoid_cnt++;
                forward_cnt = 0;
                if (avoid_cnt >= 2)
                {
                    g_app_state = STATE_AVOID;
                    reset_debounce();
                    break;
                }
            }
            /* 好转 → 恢复前进 */
            else if (dist >= SAFE_DIST_CM)
            {
                forward_cnt++;
                avoid_cnt = 0;
                if (forward_cnt >= 3)
                {
                    g_app_state = STATE_FORWARD;
                    reset_debounce();
                    break;
                }
            }
            /* 保持警惕 */
            else
            {
                forward_cnt = 0;
                avoid_cnt   = 0;
            }

            /*
             * 🎯 偏转角计算：
             *   距离越近，转向角度越大。
             *   公式：offset = (120 - dist) × 0.6
             *
             *   比如距离 100cm → (120-100)×0.6 = 12°（轻微右转）
             *   距离 50cm  → (120-50)×0.6  = 42° → 限幅到 35°（大幅右转）
             *
             *   注意：这里舵机只往"右"打。因为我们的超声波在正前方,
             *   暂时只能看到一个方向。更高级的可以扫左/中/右。
             */
            {
                int16_t offset = 0;
                if (dist >= 0 && dist < SAFE_DIST_CM)
                    offset = (int16_t)((SAFE_DIST_CM - dist) * 0.6f);
                // 限幅：确保不超过舵机最大偏转角
                if (offset >  MAX_SG90_ANGLE) offset =  MAX_SG90_ANGLE;
                if (offset < -MAX_SG90_ANGLE) offset = -MAX_SG90_ANGLE;
                g_SG90_angle = (int8_t)offset;  // 更新全局变量
                SG90_SetAngle(g_SG90_angle);     // 通知舵机
            }

            /* 减速前进 */
            DRV8833_Forward(CAUTION_PWM);  // 55% 速度，比正常慢
            g_DRV8833_dir = 1;
            g_DRV8833_pwm = CAUTION_PWM;
            break;

        /* ---- 状态：避障（AVOID） ---- */
        case STATE_AVOID:
            /*
             * 避障状态：障碍物近在咫尺！
             * 行为：立即后退 + 舵机右打最大角度
             *
             * 退出条件：距离 ≥ 80cm（连续 5 次确认安全）
             * 然后进入 RECOVERY（恢复）状态——慢慢回正方向，不要急转弯。
             */
            if (dist >= CAUTION_DIST_CM)
            {
                recovery_cnt++;
                if (recovery_cnt >= 5)  // 连续5次确认安全了
                {
                    g_app_state = STATE_RECOVERY;  // 进入恢复
                    reset_debounce();
                    break;
                }
            }
            else
            {
                recovery_cnt = 0;  // 还危险，继续倒车
            }

            /* 执行倒车+转向 */
            DRV8833_Reverse(REVERSE_PWM);     // 后退！
            g_DRV8833_dir = -1;                // 方向=后退
            g_DRV8833_pwm = REVERSE_PWM;       // 速度=65
            g_SG90_angle  = MAX_SG90_ANGLE;    // 右打满 35°
            SG90_SetAngle(MAX_SG90_ANGLE);
            break;

        /* ---- 状态：恢复（RECOVERY） ---- */
        case STATE_RECOVERY:
            /*
             * 恢复状态：刚刚脱离危险，正在慢慢回正方向。
             * 行为：减速前进 + 舵机逐渐回中（角度=0）
             *
             * 退出条件：距离 > 100cm 且 舵机已经回中（角度=0）
             * 连续 3 次满足 → 恢复 FORWARD
             */
            if (dist > 100 && g_SG90_angle == 0)
            {
                forward_cnt++;
                if (forward_cnt >= 3)
                {
                    g_app_state = STATE_FORWARD;  // 恢复前进
                    reset_debounce();
                    break;
                }
            }
            else
            {
                forward_cnt = 0;  // 还没完全恢复，继续等待
            }

            /* 减速前进，舵机慢慢回中 */
            DRV8833_Forward(CAUTION_PWM);  // 先慢速走
            g_DRV8833_dir = 1;
            g_DRV8833_pwm = CAUTION_PWM;
            g_SG90_angle  = 0;             // 舵机目标回中
            SG90_SetAngle(0);              // 舵机会平滑地转回中间
            break;

        /* ---- 状态：故障（FAULT） ---- */
        case STATE_FAULT:
            /*
             * 故障状态：超声波连续超时，传感器可能出问题了。
             * 行为：电机停止，舵机回中，等待恢复。
             *
             * 退出条件：连续 3 次读到有效距离 → 传感器恢复了
             * 回到 IDLE（空闲）状态，需要重新按 KEY1 启动。
             */
            if (dist >= 0)
            {
                fault_cnt++;
                if (fault_cnt >= 3)  // 连续3次有效数据
                {
                    g_app_state = STATE_IDLE;  // 传感器恢复了！
                    reset_debounce();
                }
            }
            else
            {
                fault_cnt = 0;  // 还是无效数据，保持故障
            }

            /* 安全第一：停止所有动作 */
            DRV8833_Stop();
            g_DRV8833_dir = 0;
            g_DRV8833_pwm = 0;
            g_SG90_angle  = 0;
            SG90_SetAngle(0);
            break;

        /* 未知状态（理论上不会执行到这里） */
        default:
            g_app_state = STATE_IDLE;  // 复位到空闲
            reset_debounce();
            break;
    }  /* switch 结束 */

    /*
     * ---- 全局故障检测 ----
     * 在 IDLE 和 FAULT 状态之外的任何运行状态（FORWARD/CAUTION/AVOID/RECOVERY）,
     * 如果连续 5 次测距都超时（dist == -1），认为传感器故障,
     * 强制进入 FAULT 状态。
     *
     * 为什么单独拎出来？
     *   因为每个状态都要写一套"超时检测"太冗余了。
     *   统一在这里处理，所有状态都能受益。
     */
    if (g_app_state != STATE_FAULT && g_app_state != STATE_IDLE)
    {
        if (dist == -1)  // -1 表示超时
        {
            fault_cnt++;
            if (fault_cnt >= 5)  // 连续5次超时
            {
                g_app_state = STATE_FAULT;  // 传感器故障！
                reset_debounce();
            }
        }
        else
        {
            fault_cnt = 0;  // 有数据，不是故障
        }
    }

    /* ---- Step 6: 舵机平滑更新 ---- */
    /*
     * SG90_Update() 执行指数平滑算法：
     * 舵机不会"瞬间跳"到目标角度，而是每次靠近一点点。
     * 这样转向柔和，不会"咔咔"地突然转过去。
     * 原理见 SG-90.c 的注释。
     */
    SG90_Update();

    /* ---- Step 7: OLED 刷新 ---- */
    /*
     * 每 500ms 刷新一次屏幕（不是每 50ms 都刷）。
     * 为什么？因为 OLED 刷新需要 I2C 通信，比较耗时。
     * 而且人眼看 500ms 刷新一次已经足够流畅了。
     * 这叫"降低刷新率以节省 CPU 时间"。
     */
    if (now - last_display_tick >= 500)
    {
        last_display_tick = now;     // 记录本次刷新时间
        OLED_NewFrame();              // 清空显存（重新画）
        update_oled();                // 画上最新数据并发送显示
        USART_LogState(state_name(g_app_state), g_distance_cm,
                       g_DRV8833_dir, g_DRV8833_pwm, g_SG90_angle);
    }
}

/*
 * ================================================================
 * 兼容旧接口
 * ================================================================
 *
 * main.c 中调用的是 App_Init() 和 App_Loop()（首字母大写）。
 * 这里提供两个"包装函数"，把大写的旧接口映射到小写的新接口。
 * 这样 main.c 不用改，保持了兼容性。
 */
void App_Init(void) { app_init(); }  // 大写版初始化 → 调小写版
void App_Loop(void) { app_run(); }   // 大写版主循环 → 调小写版
