/*电机驱动模块*/

#include "DRV8833.h"

/* ===== 模块内部状态（单实例版） =====
 * static 的意思是“只在本文件可见”。
 * 这样做的好处：外部文件改不了这些变量，误操作更少。
 */
static TIM_HandleTypeDef *s_htim = NULL;  /* TIM 句柄：告诉 HAL 我们要操作哪一个定时器 */
static uint32_t s_ch_fwd = TIM_CHANNEL_1; /* 正转通道：默认 CH1（可在 DRV8833_Init 里改） */
static uint32_t s_ch_rev = TIM_CHANNEL_2; /* 反转通道：默认 CH2（可在 DRV8833_Init 里改） */

static uint32_t duty_to_ccr(uint8_t duty_percent)
{
    /*
     * 输入：duty_percent（0..100）
     * 输出：CCR 的数值（用于 __HAL_TIM_SET_COMPARE）
     *
     * 注意：这里不做 HAL_Delay，不做等待；这是“纯换算函数”。
     */
    if (s_htim == NULL)
    {
        return 0U;
    }

    /*
     * PWM 真实周期是 (ARR + 1)。
     * 例如 CubeMX 配置 Period=100-1 => ARR=99 => 周期=100 个计数。
     * 若用 ARR 直接换算，会导致 99% 实际变成 98%（99*99/100=98）。
     */
    /* period = ARR + 1：这是 PWM 的真实周期计数（比如 ARR=99 -> period=100） */
    uint32_t period = (uint32_t)__HAL_TIM_GET_AUTORELOAD(s_htim) + 1U;
    if (period == 0U)
    {
        return 0U;
    }

    if (duty_percent >= 100U)
    {
        /* 100%：让 CCR >= ARR+1，使 CNT 永远 < CCR -> 输出全程为高（PWM1 常见边界用法） */
        return period;
    }

    /*
     * 四舍五入：
     * - 整数除法会直接向下取整
     * - +50 再 /100 就相当于把结果“四舍五入到最接近的整数”
     *   （因为分母是 100，所以加 50 是常见技巧）
     */
    return (period * (uint32_t)duty_percent + 50U) / 100U;
}

static void DRV8833_Drive(uint32_t ch_on, uint32_t ch_off, uint8_t duty_percent)
{
    /*
     * ch_on  : 这次要“输出 PWM” 的通道
     * ch_off : 这次要“确保关闭” 的通道
     * duty_percent：占空比百分比
     */
    if (s_htim == NULL)
    {
        return;
    }

    /* 0%：直接当作“停机”，比继续 Start 然后 compare=0 更清晰 */
    if (duty_percent == 0U)
    {
        DRV8833_Stop();
        return;
    }

    /* 先把百分比换算成 CCR（寄存器语义） */
    uint32_t ccr = duty_to_ccr(duty_percent);

    /* 严格“只开一路 PWM”：先关另一侧，并把 compare 清 0，避免残留脉冲 */
    HAL_TIM_PWM_Stop(s_htim, ch_off);
    __HAL_TIM_SET_COMPARE(s_htim, ch_off, 0U);

    /* 先写 compare 再 start：让第一次输出就带着正确占空比 */
    __HAL_TIM_SET_COMPARE(s_htim, ch_on, ccr);
    HAL_TIM_PWM_Start(s_htim, ch_on);
}

/* 注意：DRV8833 两路同时为 1 可能是制动；这里严格“只开一路PWM” */
void DRV8833_Init(TIM_HandleTypeDef *htim, uint32_t ch_fwd, uint32_t ch_rev)
{
    /*
     * 这个函数通常只在上电初始化调用一次：
     * - htim：例如 &htim2
     * - ch_fwd/ch_rev：例如 TIM_CHANNEL_1 / TIM_CHANNEL_2
     */
    s_htim = htim;
    s_ch_fwd = ch_fwd;
    s_ch_rev = ch_rev;

    /* 初始化后先停机：避免上电瞬间电机误转 */
    DRV8833_Stop();
}

void DRV8833_Forward(uint8_t duty_percent)
{
    /* 对外 API：业务只需要关心“正转 + 占空比” */
    DRV8833_Drive(s_ch_fwd, s_ch_rev, duty_percent);
}

void DRV8833_Reverse(uint8_t duty_percent)
{
    /* 对外 API：业务只需要关心“反转 + 占空比” */
    DRV8833_Drive(s_ch_rev, s_ch_fwd, duty_percent);
}

void DRV8833_Stop(void)
{
    /* 主动刹车：两路全开100% → DRV8833 IN1=1, IN2=1 → Brake */
    if (s_htim == NULL)
    {
        return;
    }

    uint32_t period = (uint32_t)__HAL_TIM_GET_AUTORELOAD(s_htim) + 1U;
    __HAL_TIM_SET_COMPARE(s_htim, s_ch_fwd, period);  /* 100% duty = always HIGH */
    __HAL_TIM_SET_COMPARE(s_htim, s_ch_rev, period);  /* 100% duty = always HIGH */
    HAL_TIM_PWM_Start(s_htim, s_ch_fwd);              /* 确保两路都在输出 */
    HAL_TIM_PWM_Start(s_htim, s_ch_rev);
}

#if 0
教学例程搬运（仅留档参考，不参与编译）

        // KEY1按下：占空比99% 高速正转
            if (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
            {
                // 启动PWM通道1输出（只能同时启动1个通道，两个通道对应正/反转）
                HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
                // 配置通道1的占空比，影响电机转速（占空比过低可能导致电机无法启动）
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 99);
            }
            // KEY2按下：占空比85% 中速正转
            else if (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
            {
                // 启动PWM通道1输出（只能同时启动1个通道，两个通道对应正/反转）
                HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
                // 配置通道1的占空比，影响电机转速（占空比过低可能导致电机无法启动）
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 85);
            }
            else
            {
                // 停止PWM通道1输出
                HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
            }
            HAL_Delay(100);
#endif