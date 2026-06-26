# MY_OTA_GUI — 开发进度与踩坑记录

> 2026-06-25 | STM32F407ZGT6 + NT35510 LCD 800×480 + LVGL v9.3 + W25Q128 SPI Flash

---

## 一、最终成果

| 项目 | 状态 | 说明 |
|------|------|------|
| LVGL v9.3 渲染 | ✅ | 单缓冲 50KB (BUF_LINES=32), PARTIAL 模式 |
| SPI Flash 批量烧录 | ✅ | 26/26 headers 全通过, SPI 42MHz 稳定 |
| **主页** moto 800×480 背景 | ✅ | `S:` 路径流式解码 |
| **主页** 小组件 (方向灯/远光灯/发动机等) | ✅ | 外部 SRAM 预加载 (~128KB) |
| **Mode 屏** 4 tile 纯色背景 | ✅ | LVGL 样式颜色, 秒切无延迟 |
| **Mode 屏** 小图 (指南针/温度/天气) | ✅ | 外部 SRAM 预加载 (~83KB) |
| **Mode 屏** 音乐封面 | ✅ | `S:` 路径流式解码 (183×183) |
| 触摸屏 (GT911) | ✅ | 软件 I2C (PB0/PF11), 独立 task 100Hz 轮询 |
| Home↔Mode 切换 | ✅ | 触控点击 + 左右滑动手势 |
| 外部 SRAM | ⚠️ | 线性格子 ≤256KB 安全; 全帧缓冲实测不可靠 |
| moto 背景 y≈240 水平线 | ⚠️ | 小瑕疵, 不影响使用 |

---

## 二、触摸驱动 — GT911 完整调试链路

### 硬件引脚
| 引脚 | 功能 | 说明 |
|------|------|------|
| PB0 | I2C SCL | 软件 bit-bang |
| PF11 | I2C SDA | 动态切换输入/输出 |
| PC13 | RST | 备份域引脚, 需 `HAL_PWR_EnableBkUpAccess()` |
| PB1 | INT | CubeMX 已初始化为 INPUT_PULLUP |

### 关键修复
| # | 问题 | 修复 |
|---|------|------|
| 1 | I2C Wait_Ack 未释放总线 | 切换输入后先写 `SDA_H()` 再发第9时钟 |
| 2 | I2C Read_Byte 缺稳定延时 | 切换输入后加 30μs |
| 3 | PB1 被设为 OUTPUT_PP (与 GT911 输出冲突) | 移除重设, 保留 CubeMX INPUT_PULLUP |
| 4 | 坐标变换写反 (GT5663≠GT911) | 改为 `X=800-chip_y, Y=chip_x` |
| 5 | LVGL indev timer 被 600ms 渲染拖慢 | 独立 FreeRTOS task (AboveNormal, 10ms) 解耦 |
| 6 | 编译优化 -O0 渲染极慢 | 切 -Og |

### 触控架构
```
touchTask (AboveNormal, 10ms)      guiTask (Normal, 不定时)
  └→ touch_poll()                    └→ lv_timer_handler()
       └→ GT5663_Scan (I2C ~4ms)          └→ touch_read_cb()
       └→ 更新 volatile 缓存               └→ 读缓存 (瞬间)
       └→ LED 控制
```

---

## 三、外部 SRAM — 深度诊断

### 硬件事实
- 芯片: IS62WV51216 (512K×16bit = 1MB), FSMC Bank3 @ 0x68000000
- PCB 地址线交叉: `SRAM_A10 ↔ FSMC_A17, SRAM_A17 ↔ FSMC_A10`
- GPIO Pull: 官方 sram.c 要求 **所有 FSMC 引脚 PULLUP** (已对齐)

### 测试结果
| 测试 | 方法 | 结果 |
|------|------|------|
| 16-bit 字诊断 | 个别地址写/读 | ✅ PASS |
| 块扫描 (256B~64KB) | 逐块写+立即读 | ✅ 全 1MB PASS |
| 连续大缓冲写入 | 768KB 帧缓冲 | ❌ 梯度验证 83% MISMATCH |
| 线性 buffer 可用量 | 写入 12 张小图 | ✅ ≤256KB 稳定 |

### 结论
- **单块 ≤64KB + DSB 隔离**: 贯穿全 1MB 可靠
- **连续 >256KB 线性 buffer**: 后写入会物理覆盖前写入的数据
- **根因**: 地址交叉产生的物理映射在连续大范围访问时不稳定
- **实用上限**: 线性 buffer ≤256KB, 非连续小块任意分布

---

## 四、架构决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 绘制缓冲 | 内部 SRAM1 50KB, PARTIAL 模式 | 全帧 DIR EC T 在外部 SRAM 实测不可靠 |
| 大图 (>200KB) | S: 路径流式解码 | moto 800×480, det_bak, music 封面 |
| 小图 (<128KB) | 外部 SRAM 预加载, ≤256KB 总和 | 12 张图 ~211KB, VARIABLE 源零拷贝 |
| SPI Flash 速度 | 42MHz (BR=0, fPCLK/2) | 理论上限, 稳定运行 |
| FSMC GPIO | PULLUP + HIGH speed | 对齐官方 sram.c |
| 编译优化 | -Og | -O0 太慢, -O2 调试困难 |
| Mode 屏背景 | 纯 LVGL 颜色 | 避免 tile 切换时 S: 解码延迟 |

---

## 五、图片分布总表

| 图片 | 尺寸 | 来源 | 位置 |
|------|------|------|------|
| moto | 800×480 | `S:001F5F2C:0011940C.bin` | 主页全屏背景 |
| kmbg | 166×166 | SRAM | 主页速度表背景 |
| direction | 50×42 | SRAM | 左右转向灯 |
| monitor | 41×37 | SRAM | 发动机图标 |
| light | 46×46 | SRAM | 灯光 |
| high_beam | 46×46 | SRAM | 远光灯 (默认隐藏) |
| battery_bak/ind | 43×24 | SRAM | 电池条 |
| home | 43×43 | SRAM | 返回主页图标 |
| logo | 100×35 | SRAM | |
| nav | 116×116 | SRAM | Mode-A 指南针 |
| temper | 91×123 | SRAM | Mode-F 温度计 |
| weather | 66×66 | SRAM | Mode-C 天气图标 |
| music_cover_1 | 183×183 | `S:0030F338:00018880.bin` | Mode-E 专辑封面 |
| det_bak | 800×480 | 已弃用 | tile 背景替换为纯色 |

---

## 六、关键踩坑

### 坑1: main_batch.c 缺少中断向量表 → MCU 不启动
- **现象**: ST-Link 烧录 batch 固件后 MCU 不运行
- **根因**: `main_batch.c` 没有 `.isr_vector` 段
- **修复**: 添加 16 系统异常 + 16 IRQ 向量表 + `Reset_Handler`

### 坑2: `lv_bin_decoder` 要求 `.bin` 扩展名
- **现象**: `"S:001F5F2C:0011940C"` 无法解码
- **根因**: `lv_bin_decoder.c` 检查文件扩展名为 `.bin`
- **修复**: 路径加 `.bin` 后缀 — sscanf 只读 hex 部分

### 坑3: I2C 协议对齐官方例程
- **现象**: GT911 返回全 0xFF, 芯片不响应
- **修复**: Wait_Ack 释放总线 + Read_Byte 稳定延时 + PB1 不驱 OUTPUT

### 坑4: 触控坐标映射错误
- **现象**: 按底部按钮但坐标显示在屏幕顶部
- **根因**: GT911 的 X/Y 轴方向与 GT5663 不同
- **修复**: `X=800-chip_y, Y=chip_x`

### 坑5: 外部 SRAM 全帧缓冲失败
- **现象**: DIRECT 模式 768KB 帧缓冲梯度验证 83% 错配
- **尝试**: volatile / __DSB / 分块 / AddressSetupTime — 均无效
- **结论**: 该板子 SRAM 不支持全帧缓冲, 回退 PARTIAL 模式

### 坑6: 外部 SRAM 地址交叉
- **现象**: 写入 >256KB 后, 低地址数据被覆盖
- **根因**: `SRAM_A10 ↔ FSMC_A17` PCB 交叉走线
- **对策**: 线性 buffer ≤256KB

---

## 七、待完成

1. **moto 背景 y≈240 水平线**: 小瑕疵, 不影响功能
2. **Mode 屏美化**: 用户计划用 LVGL 样式进一步美化背景
3. **功能组件对接**: 速度/里程/电池等数据源未接入 (当前为演示动画)
