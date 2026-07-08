# MY_OTA_GUI — 工程全历程与踩坑记录（已弃用工程并被覆盖，次文档仅作开发失败保留文档）

> 2026-06-25 ~ 2026-06-29 | STM32F407ZGT6 + NT35510 LCD 800×480 + LVGL v9.3 + W25Q128 SPI Flash
> 作者：YHapcalis

---

## 一、项目时间线

```
2026-06-25 — 项目启动，基础硬件验证
2026-06-26 — LVGL 仪表盘 UI 构建、触控调试、SRAM 诊断
2026-06-27 — F103 工程搭建、AHT20 温湿度、CAN 通信打通
2026-06-28 — F407 架构优化 5 轮重构、转向灯、布局常量化
2026-06-29 — 电机/舵机控制尝试 ×3 方案均失败，工程总结
```

---

## 二、最终成果总表

| 模块 | 状态 | 说明 |
|------|------|------|
| LCD NT35510 驱动 800×480 | ✅ | FSMC 8080 16-bit, 帧率流畅 |
| LVGL v9.3 渲染 | ✅ | PARTIAL 模式, 25KB 缓冲 (BUF_LINES=16), RAM 79% |
| 触摸 GT911 | ✅ | 软件 I2C PB0/PF11, 独立 FreeRTOS task 100Hz |
| SPI Flash W25Q128 | ✅ | 42MHz, 26/26 图片全部验证通过 |
| S: 路径流式解码 | ✅ | moto 背景、音乐封面通过 SPI Flash 流式加载 |
| 外部 SRAM 图片缓存 | ✅ | 12 张小图 ~211KB 预加载到 SRAM |
| Home 屏 UI | ✅ | 速度表、转向灯、电池、时钟、4 功能按钮 |
| Mode 屏 UI | ✅ | 4 tile (NAV/TEMP/WEATHER/MUSIC), 滑动手势 |
| Home↔Mode 切换 | ✅ | 触控点击 + 左右滑动手势, 无卡死 |
| CAN 通信 (F103↔F407) | ✅ | 500kbps, 温度/湿度/旋钮/按键 实时传输 |
| 旋钮→转向灯 | ✅ | 左旋左灯闪, 右旋右灯闪, 3 秒自动熄灭 |
| AHT20 温湿度采集 | ✅ | F103 本地采集, 通过 CAN 传至 F407 TEMP 屏 |
| **工程架构重构** | **✅ 5 轮** | freertos.c→custom.c→hw_diag.c, 死代码清理, 布局常量化 |
| **电机 DRV8833 实时控制** | **❌ 放弃** | 三个方案均因电源/总线/调度问题失败 |
| **舵机 SG-90 实时控制** | **❌ 放弃** | 同上 |

---

## 三、工程架构演进

### v0 — 原始状态（GUI-Guider 生成骨架）
```
freertos.c = 百宝袋（RTOS 任务 + UI 回调 + 300 行诊断 + 自模拟动画）
custom.c  = 杂物间（50 行死代码 + 占位音乐播放器 + chart 定时器）
所有坐标 = 散落的魔法数字
```

### v1 — 5 轮重构后
```
main.c ─── 薄入口, 仅 HAL 初始化和 App 启动
  │
  └→ freertos.c ─── 仅 RTOS 任务管理 (~188 行, 原 320 行)
       ├→ StartGUITask      GUI 初始化 + LVGL event loop
       ├→ StartTouchTask    触控轮询 100Hz
       ├→ StartCanRxTask    CAN 接收轮询 50Hz
       └→ StartDefaultTask  LED 心跳 + UART 回显
  │
  └→ custom.c ─── LVGL 回调 + 业务逻辑
       ├→ mode_temp_update_cb     CAN 数据显示 (当前闲置)
       ├→ home_turn_signal_cb     转向灯控制 (100Hz)
       ├→ f407_key_motor_servo_cb 按键扫描 (当前闲置)
       └→ speed_meter_timer_cb    仪表盘模拟
  │
  └→ hw_diag.c ─── 独立硬件诊断模块 (原嵌在 freertos.c)
  └→ BSP/Canif/canif.c/h ─── CAN 接口层
  └→ BSP/Driver/ ─── DRV8833 + SG-90 驱动
  └→ layout_defines.h ─── 68 个布局常量 (替代魔法数字)
```

### 重构改动清单
| 重构动作 | 涉及文件 | 行数变化 |
|---------|---------|---------|
| `mode_temp_update_cb` → `custom.c` | freertos.c, custom.c | -40 |
| 删除 `digital_cluster_chart_timer_cb` | custom.c | -52 |
| 删除 `reset_music_status` | custom.c, custom.h | -5 |
| 删除 `#define CHART1_POINTS` + `spd_chart[40]` + `is_up` | custom.c | -3 |
| `hw_diag()` → 独立 `hw_diag.c` | freertos.c, hw_diag.c | -164 |
| `_write` → `usart.c` | main.c, usart.c | -6 |
| `canif.h` 注释过期修正 | canif.h | 1 行 |
| 魔法数字 → `layout_defines.h` | setup_scr_home.c, setup_scr_mode.c | 68 个常量 |
| `guider_ui` + 3 个 Task handle 恢复 | freertos.c | CubeMX 覆盖后手动补回 |
| **总计改动 17 个文件** | | |

---

## 四、CAN 通信完整排错

### 4.1 引脚配置差异（最大坑）

```
F103: CAN_RX = GPIO_MODE_INPUT         ← CAN 外设自动接管
      CAN_TX = GPIO_MODE_AF_PP

F407: CAN_RX = GPIO_MODE_AF_PP + AF9   ← 必须通过 AF 路由！
      CAN_TX = GPIO_MODE_AF_PP + AF9
```

**这个差异导致了 2 小时的排查。** 我用 F103 的经验修 F407，把 PA11 配成了 INPUT，CAN 控制器收不到任何信号，但 ESR 寄存器全零（无错误），最难排查的情况。

### 4.2 CubeMX 重新生成的破坏 (2026-06-29)

用户为了添加 TIM8/TIM11 (电机舵机 PWM) 重新生成了 CubeMX 代码，导致：

| 破坏 | 恢复方式 |
|------|---------|
| CAN1_RX 从 PA11 改为 **PB8** | 手动改回 PA11 |
| CAN1_RX/TX 分两次配置，残留 `HAL_GPIO_Init(GPIOB, ...)` | 删掉 GPIOB 行 |
| `guider_ui` 定义丢失 | 在 `freertos.c` 补回 `lv_ui guider_ui;` |
| `guiTaskHandle` / `touchTaskHandle` / `canRxTaskHandle` 定义丢失 | 在 `USER CODE BEGIN Variables` 补回 |
| `main.c` 缺少 `while(1)` 的右大括号 | 补 `}` |

### 4.3 CAN 帧格式演进

| 版本 | 帧内容 | 频率 | 用途 |
|------|--------|------|------|
| v1 | `[temp_int+40, hum_int, knob, key]` | 1Hz | 初版通信 |
| v2 | `[temp_int+40, temp_dec, hum_int, hum_dec, knob, key]` | 1Hz | 0.1°C 精度 |
| v3 | `[0, 0, 0, 0, knob, key_id<<4\|type]` | **50Hz** | 尝试实时控制 → 失败回退 |
| v4 (最终) | `[0, 0, 0, 0, knob, key_id<<4\|type]` | 1Hz | 仅仪表盘显示 |

### 4.4 过滤器配置
全接收模式 (`FilterMaskIdHigh/Low = 0`)，不过滤任何 ID，软件层区分。

---

## 五、电机/舵机实时控制 — 三次尝试全记录

### 方案 A: F103 本地直驱 (2026-06-27)

**接线：**
```
F103 PA0/PA1 → DRV8833 → 电机
F103 PB8     → SG-90 舵机
F103 + TJA1050 独立 CAN 模块
```

**现象：**
- 电机和舵机能转 ✅
- 运行几分钟后 CAN 模块 **发烫烧毁** ❌

**根因：**
```
F103 5V (USB 500mA) ──┬── SG-90 舵机 (峰值 500mA)
                       ├── TJA1050 CAN 模块 (~50mA)
                       └── STM32F103 开发板 (~100mA)

舵机急转时拉走 500mA → 5V 跌落 → TJA1050 低于工作电压
→ CAN 帧错误 → 总线关闭 → 模块持续高电流 → 过热烧毁
```

**结论：** F103 的 5V 供电不足，不能同时驱动舵机+电机+CAN。

### 方案 B: F407 远程 CAN 指令控制 (2026-06-29)

**架构：**
```
F103 (旋钮+按键) → CAN ID=0x12 (50Hz) → F407 → DRV8833 + SG-90
```

**尝试过程：**
```
① CAN 1Hz 发送 → 舵机每秒才动一次，完全无法接受
② CAN → 20ms (50Hz) → 舵机响应有可感知的抖动
③ CAN → 20ms + SG90_Update 指数平滑 → 仍有迟滞感
```

**根因：**
```
CAN 总线的天生限制：
  1. 仲裁延迟 — 高优先级帧抢占，不可预测
  2. 传输延迟 — 即使 500kbps, 1 帧 ≈ 0.1ms, 但 FreeRTOS 轮询周期 20ms
  3. 调度抖动 — canRxTask 优先级与 GUI task 竞争
  
50Hz 的 CAN 更新率对于舵机伺服控制远不够
(真实舵机控制需要 100Hz+ 的确定性脉冲)
```

**结论：** CAN 总线不适合实时伺服控制。这是协议层的根本限制，不是代码能解决的。

### 方案 C: F407 本地按键直驱 (2026-06-29)

**接线：**
```
F407 PC6/PC7 (TIM8) → DRV8833 → 电机
F407 PF7 (TIM11)    → SG-90 舵机
F407 PE2/PE3/PE4    → KEY2/KEY1/KEY0 本地按键
F407 PA0            → KEY_UP (WK_UP)
```

**现象：**
- 按键响应正常 ✅
- KEY_UP 按下一瞬间电机启动 → **LCD 花屏** ❌
- 花屏表现为横条纹线状干扰

**根因：**
```
电机启动电流 → 5V 瞬时跌落 → FSMC 总线噪声
→ NT35510 LCD 数据线受干扰 → 显示错乱

F407 虽然供电比 F103 好，但电机和 LCD 共用同一路 5V
电机瞬间电流 ~500mA 仍能拉低 VCC5
```

**尝试的缓解措施（均无效）：**
```
1. 注释 moto 背景 (S: 路径流式解码) → 花屏依旧
2. 降低电机 PWM 占空比 80%→50% → 花屏依旧
3. 纯黑背景 (bg_opa=255) → 花屏依旧
```

**结论：** 需要独立电源给电机供电，与液晶/F407 隔离。

---

## 六、关键踩坑 20 条

### #1: main_batch.c 缺少中断向量表 → MCU 不启动
- **场景**: SPI Flash 批量烧录固件
- **现象**: ST-Link 烧录后 MCU 不运行
- **根因**: `main_batch.c` 没有 `.isr_vector` 段，CPU 启动后直接跑飞
- **修复**: 添加 16 系统异常 + 16 IRQ 向量表 + `Reset_Handler`

### #2: `lv_bin_decoder` 要求 `.bin` 扩展名
- **场景**: S: 路径加载图片
- **现象**: `"S:001F5F2C:0011940C"` 无法解码
- **根因**: `lv_bin_decoder.c` 检查文件扩展名是否为 `.bin`
- **修复**: 路径加 `.bin` 后缀

### #3: I2C Wait_Ack 未释放总线
- **场景**: GT911 触控驱动
- **现象**: 读取 GT911 寄存器全返回 0xFF
- **修复**: 在读取 ACK 前先写 `SDA_H()` 释放总线

### #4: GT911 坐标变换写反
- **场景**: 触控坐标映射
- **现象**: 按屏幕底部，坐标显示在顶部
- **根因**: GT911 的 X/Y 轴方向与 GT5663 不同
- **修复**: `X=800-chip_y, Y=chip_x`

### #5: 触控轮询被 LVGL 渲染阻塞
- **场景**: 触控响应极慢 (~600ms)
- **根因**: LVGL indev timer 与渲染共用同一任务，渲染阻塞了轮询
- **修复**: 独立 FreeRTOS touchTask (AboveNormal 优先级, 10ms 周期)

### #6: 编译优化 -O0 渲染极慢
- **场景**: LVGL 动画卡顿
- **根因**: -O0 关闭所有优化，CPU 跑不满
- **修复**: 切换到 -Og

### #7: 外部 SRAM 地址线交叉
- **场景**: SRAM 大块写入后数据异常
- **现象**: 写入 >256KB 后低地址数据被覆盖
- **根因**: 原理图错误: SRAM_A10↔FSMC_A17, SRAM_A17↔FSMC_A10 PCB 交叉走线
- **对策**: 连续线性 buffer 上限 256KB；块扫描 (≤64KB + DSB) 贯穿全 1MB 可靠

### #8: 外部 SRAM 全帧缓冲 83% 错配
- **场景**: 尝试使用外部 SRAM 做 LVGL DIRECT 全帧缓冲 (768KB)
- **尝试**: volatile 指针 / __DSB() 分块写入 / AddressSetupTime=15 — 全部无效
- **结论**: 该板 SRAM 不支持全帧缓冲，回退 PARTIAL 模式，内部 SRAM 50KB

### #9: FSMC GPIO 速度与上拉
- **场景**: 块扫描验证时数据偶发损坏
- **根因**: 与官方 sram.c 不一致 — GPIO 没配 PULLUP，速度不够 HIGH
- **修复**: 所有 FSMC 引脚 `PULLUP | SPEED_FREQ_HIGH`

### #10: newlib-nano `%f` 不显示
- **场景**: F103 串口打印 `T=C H=%` (空白浮点)
- **根因**: `--specs=nano.specs` 默认不包含 `%f` 格式化代码
- **修复**: 链接器加 `-Wl,-u,_printf_float`

### #11: F103 printf 不输出 (`__io_putchar` 未定义)
- **场景**: F103 串口无输出
- **根因**: `syscalls.c` 中的 `_write()` 调用了 `__io_putchar`，但该函数是 weak 空符号
- **修复**: 在 `usart.c` 的 `USER CODE BEGIN 1` 中实现 `__io_putchar()`

### #12: Mode→Home 切换卡死
- **场景**: 从 Mode 屏滑回 Home 屏时系统卡死
- **根因**: `mode_temp_update_cb` LVGL timer 在 Mode 屏被销毁后继续访问已释放的 widget 指针
- **修复**: 回调入口加 `lv_screen_active() != guider_ui.mode` 保护

### #13: F407 CAN_RX 配成 INPUT（最大坑）
- **场景**: CAN 通信不通，ESR 全零
- **根因**: 用 F103 经验处理 F407，两者 CAN_RX 的 GPIO 模式不同
- **修复**: F407 CAN_RX 必须 `GPIO_MODE_AF_PP` + `GPIO_AF9_CAN1`

### #14: CubeMX 把 CAN1_RX 改成 PB8
- **场景**: CubeMX 重新生成后 CAN 恢复不了
- **根因**: CubeMX 把 CAN1_RX 引脚从 **PA11** 改成了 **PB8**，板载 TJA1040 只连到 PA11
- **修复**: 每次 CubeMX 生成后检查 `HAL_CAN_MspInit` 的引脚配置

### #15: CubeMX 残留 `HAL_GPIO_Init(GPIOB, ...)`
- **场景**: CAN 引脚修复后仍然不通
- **根因**: 修改 PB8→PA11 时没删干净，残留一行 `HAL_GPIO_Init(GPIOB, &GPIO_InitStruct)`，错误地配置了 PB12
- **修复**: 删除该残留行

### #16: FSMC 覆盖按键引脚
- **场景**: PE3(KEY1) 不响应
- **根因**: FSMC 将 GPIOE 0-4 全部配为 AF_PP，覆盖了之前的 INPUT 配置
- **修复**: `MX_FSMC_Init()` 之后重新配置 PE2-PE4 为 `GPIO_MODE_INPUT | GPIO_PULLUP`

### #17: PE3(KEY1) 缺少宏定义
- **场景**: PE3 没有 KEY1_Pin/KEY1_GPIO_Port 宏
- **根因**: CubeMX 默认没有为 KEY1 生成宏
- **修复**: 在 `main.h` 中手动添加 `#define KEY1_Pin GPIO_PIN_3 / KEY1_GPIO_Port GPIOE`

### #18: CAN 50Hz 数据淹没串口
- **场景**: 将 CAN 从 1Hz 提速到 50Hz 后串口疯狂打印
- **根因**: `canif_parse_sensor` 每收到一帧就打印一次，50Hz = 每秒 50 行
- **修复**: 去掉 `canif_parse_sensor` 中的 `printf`

### #19: RAM 98.82% 十几分钟后死机
- **场景**: F407 运行 ~15 分钟后卡死，复位恢复
- **根因**: LVGL 绘制缓冲 50KB 占用过多，加上堆碎片化，RAM 只剩 1.5KB
- **修复**: `BUF_LINES` 32→16 (50→25KB), GUI 任务栈 8→4KB, 降至 79.54%

### #20: CAN 模块发烫烧毁
- **场景**: F103 直连电机舵机运行几分钟后
- **根因**: F103 USB 5V 供电不足 → `5V 跌落 → TJA1050 欠压 → CAN 总线错误 → 总线关闭 → 收发器持续高电流 → 过热`
- **对策**: 电机舵机使用独立供电，不与 CAN 共用 5V

---

## 七、电机/舵机失败的根因总结

三个方案，三种失败方式，同一个根因：**电源**

```
方案  ── 失败方式 ──  根因
─────────────────────────────────
F103 直驱  → CAN 烧毁 → 5V 供电不足
F407 CAN   → 响应延迟 → CAN 协议不适合实时控制
F407 按键  → LCD 花屏 → 电机与 LCD 共电源，噪声耦合
```

### 如果要继续，必须解决的前提条件

| 条件 | 方案 |
|------|------|
| 电机/舵机独立电源 | 18650 电池组或独立 5V 电源模块 |
| 电机与 LCD 隔离供电 | 电机电源和 F407 电源不共地 (光耦隔离) |
| 实时伺服控制 | 硬件定时器中断产生 PWM，不走 FreeRTOS/LVGL 调度 |
| CAN 仅用于状态上报 | 不再用 CAN 做控制信号 |

---

## 八、知识库归档清单

| 文件 | 说明 | 最后更新 |
|------|------|---------|
| `PROJECT_PLAN.md` | 工程计划表 (Phase 0~8) | 2026-06-29 |
| `PROGRESS.md` | 本文件 — 完整历程 | 2026-06-29 |
| `docs/CAN.md` | CAN 通信配置手册 (引脚/时序/帧格式/排错) | 2026-06-29 |
| `Core/Inc/layout_defines.h` | 68 个布局常量 | 2026-06-29 |
| 记忆 `f407-f103-can-pin-difference` | F407/F103 CAN 引脚差异 | 2026-06-29 |
