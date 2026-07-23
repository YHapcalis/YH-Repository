# 嵌入式项目集合

> STM32F407 双区 OTA 智能仪表盘 + STM32F103 水面小船自动避障系统

---

## 项目一：MY_OTA_GUI — 双区 OTA 智能仪表盘

```
  F103C8T6（传感器节点）                  F407ZGT6（主控仪表盘）
  ┌─────────────────────┐               ┌──────────────────────────────┐
  │ AHT20 温湿度传感器    │──CAN 500kbps──│  Bootloader (64KB)           │
  │ 旋钮 + 按键输入      │   ID=0x12    │    ├─ 参数扇区校验           │
  │ RTC 时钟 (DS3231)    │─────────────→│    ├─ HMAC-SHA256 签名验证     │
  │ OLED 本地显示        │               │    ├─ CAN ISO-TP OTA 接收     │
  └─────────────────────┘               │    └─ SPI Flash 备份/回滚     │
                                        │                              │
                                        │  APP (832KB)                 │
                                        │    ├─ LVGL 8.3 图形界面     │
                                        │    ├─ 四屏 GUI（5 个 FreeRTOS 任务）│
                                        │    ├─ OV7670 摄像头预览      │
                                        │    ├─ LittleFS 文件系统      │
                                        │    ├─ ESP8266 WiFi OTA       │
                                        │    └─ 图片三级容错加载       │
                                        └──────────────────────────────┘
```

## 核心特性

| 模块 | 说明 |
|:---|:---|
| **双区 OTA** | Bootloader(64KB) + APP(832KB)，HMAC-SHA256 签名，CAN ISO-TP + WiFi TCP 双通道升级，失败自动回滚 |
| **LVGL 仪表盘** | 800×480 NT35510 LCD，4 屏 GUI（仪表盘/系统信息/设置/时钟），中文字库，指针缓动动画 |
| **CAN 通信** | 500kbps，F407↔F103 自定义协议，50Hz 传感器数据 + 1Hz RTC 时间 |
| **OV7670 摄像头** | GPIO FIFO 方案（非 DCMI），EXTI 中断驱动，QVGA 320×240 实时预览 |
| **WiFi OTA** | ESP8266 USART3 中断驱动，定向推送(TCP+UID 握手) + 广播推送(UDP 触发)双模式 |
| **图片容错** | 三级故障降级：外部 SRAM → 内部 Flash → 编译后备，启动自动检测 |
| **LittleFS** | W25Q128 16MB SPI Flash，图片存储 + 固件备份 |
| **FreeRTOS** | 5 任务（GUI/触摸/CAN/WiFi/心跳），栈空间与优先级精细调优 |

## 硬件配置

| 模块 | 接口 | 关键参数 |
|:---|:---|:---|
| 主控 | STM32F407ZGT6 | Cortex-M4 @ 168MHz, 1MB Flash, 192KB SRAM |
| 传感器节点 | STM32F103C8T6 | AHT20 + 旋钮 + 按键 + OLED + DS3231 |
| LCD | NT35510 (FSMC NE4) | 800×480, RGB565, 16-bit 8080 |
| 触摸 | GT911 (软件 I2C) | 5 点触控 |
| SPI Flash | W25Q128 (SPI1) | 16MB |
| 外部 SRAM | IS62WV51216 (FSMC NE3) | 1MB |
| CAN | CAN1 (PA11/PA12) | 500kbps |
| 摄像头 | OV7670 + AL422B FIFO | QVGA 320×240, GPIO 时序 |
| WiFi | ESP8266 (USART3) | 115200bps, AT 指令 |
| 调试串口 | USART1 (CH340C) | 115200bps |

## 快速开始

### 前置条件

- **ARM GCC 工具链**：`arm-none-eabi-gcc` (推荐 12.3+)
- **CMake**：3.22+
- **Ninja**：构建系统
- **OpenOCD**：调试与烧录（需 ST-LINK V2）
- **Python 3**：固件签名脚本

### 构建

```bash
# 配置（在工程根目录）
cmake -B build/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 编译（APP + Bootloader 并行）
cmake --build build/Debug
```

构建产物：
- `build/Debug/MY_OTA_GUI.bin` — APP 固件
- `build/Debug/bootloader.bin` — Bootloader 固件
- `build/Debug/signature.bin` — HMAC 签名块（自动生成）

### 烧录

```bash
# 全量烧录（BL + APP + 签名 + 参数）
openocd -f openocd.cfg \
  -c "program build/Debug/bootloader.elf verify" \
  -c "program build/Debug/MY_OTA_GUI.elf verify" \
  -c "program build/Debug/signature.bin 0x080DFF80 verify" \
  -c "program param_init.bin 0x080E0000 verify" \
  -c "reset; exit"
```

### SPI Flash 初始化

```bash
# 首次使用需烧录 LFS 镜像到 SPI Flash
双击运行: scripts/0-全量烧录LFS到SPIFlash.bat
```

## Flash 内存布局

```
0x08000000 ┐ Bootloader (64KB)
0x08010000 ┤
           │ APP (832KB)
0x080DFF80 ┤ 签名块 (40B: fw_size + HMAC + Magic)
0x080E0000 ┤ 参数扇区 (128KB)
0x08100000 ┘

SPI Flash (W25Q128, 16MB):
0x000000 ┐ LittleFS (14MB, 图片 + firmware.bak)
0xE00000 ┤ 裸备份区 (512KB)
0xE7FFFF ┘
```

## OTA 升级流程

```
APP 运行 → 用户触发 OTA → 备份当前固件到 SPI Flash
  → 设 ota_flag=1 → 复位
  → Bootloader 检测 flag → 进入 OTA 模式
  → [CAN ISO-TP] 接收新固件 / [WiFi] 从 SPI Flash 恢复
  → HMAC-SHA256 签名验证 → 跳转新 APP
  → 验证失败? → SPI Flash 回滚到旧版本
```

### WiFi OTA 命令

```bash
# 定向推送（TCP + UID 握手）
python scripts/wifi_ota_send.py 192.168.0.235 build/Debug/MY_OTA_GUI.bin

# 广播推送（UDP 触发 + TCP 下载）
python scripts/wifi_ota_send.py build/Debug/MY_OTA_GUI.bin --broadcast
```

## 目录结构

```
MY_OTA_GUI/
├── Bootloader/                  # BL 独立子工程（入口/ISO-TP/备份恢复）
├── Core/
│   ├── Src/
│   │   ├── APPLICATION/         # 应用层：main / freertos / UI 页面
│   │   ├── SERVICE/             # 服务层：DWT 延时 / 签名 / LFS / 参数管理
│   │   ├── DRIVER/              # 驱动层：OV7670 / NT35510 / CAN / Flash / 触控
│   │   └── gpio/usart/fsmc.c    # HAL 封装
│   ├── Inc/                     # 头文件（分区对应）
│   └── ThirdParty/littlefs/     # LittleFS 源码
├── BSP/Canif/                   # CAN 接口层
├── scripts/                     # 烧录/LFS 工具 / WiFi OTA 脚本
├── docs/                        # 详细文档
├── CMakeLists.txt               # APP + BL 双目标
└── openocd.cfg                  # OpenOCD 调试配置
```

软件架构遵循 **APPLICATION → SERVICE → DRIVER → HAL** 四层单向依赖原则。

## 文档索引

| 文档 | 说明 |
|:---|:---|
| [工程完成总结报告](docs/工程完成总结报告.md) | 完整技术细节、踩坑记录、内存分析 |
| [CAN 协议手册](docs/CAN.md) | CAN 帧格式与通信协议 |
| [VSCode 调试环境](docs/vscode-debug-环境说明.md) | 双区调试搭建 |
| [OTA 升级详解](docs/OTA升级.md) | WiFi/CAN/MQTT/HTTP 全模式 |

## 技术栈

`C` `FreeRTOS` `LVGL 8.3` `CAN 2.0B` `ISO-TP` `LittleFS` `ESP8266` `HMAC-SHA256`
`CMake` `GCC ARM` `OpenOCD` `Python` `STM32F4` `STM32F1` `FSMC` `SPI` `I2C` `UART`

---

## 项目二：MY_ObstacleCraft — 水面小船自动避障系统

> STM32F103C8T6 · 超声波测距 · 电机舵机闭环 · 分级避障状态机 · OLED 实时显示

### 系统架构

```
  HC-SR04 超声波 ──测距──► STM32F103C8T6 ──PWM──► DRV8833 电机驱动 ──► 螺旋桨推进
                                     │
                                     ├──PWM──► SG90 舵机 ──► 航向修正
                                     │
                                     ├──I2C──► 0.96" OLED ──► 距离/状态/舵角显示
                                     │
                                     └──UART──► 串口预留 (调试扩展)
```

### 核心特性

| 模块 | 说明 |
|:---|:---|
| **超声波测距** | HC-SR04，周期性获取前方障碍物距离 |
| **电机驱动** | DRV8833，支持正转/反转/停止，PWM 占空比调速 |
| **舵机控制** | SG90，中位/偏转/回中，分级角度修正 |
| **分级避障** | 根据距离区间动态调整舵角和电机占空比，近距→减速+转向+后退，解除→逐步恢复 |
| **OLED 显示** | 实时显示距离、螺旋桨状态、占空比、舵机角度、当前控制阶段 |
| **按键交互** | 预留启停/模式切换入口 |

### 项目结构

```
MY_ObstacleCraft/
├── Core/
│   ├── Src/
│   │   ├── app.c              # 避障调度主逻辑
│   │   ├── HC-SR04.c          # 超声波测距驱动
│   │   ├── DRV8833.c          # 电机驱动
│   │   ├── SG-90.c            # 舵机角度控制
│   │   ├── oled.c / font.c    # OLED 显示 + 字库
│   │   ├── key.c / key1.c     # 按键输入
│   │   ├── usart.c            # 串口驱动
│   │   └── stm32f1xx_it.c     # 中断服务
│   ├── Inc/                   # 头文件
│   └── Startup/               # 启动文件
├── Drivers/
│   ├── CMSIS/                 # Cortex-M3 CMSIS
│   └── STM32F1xx_HAL_Driver/  # STM32F1 HAL 库
├── MY_ObstacleCraft.ioc       # CubeMX 工程文件
└── 工程完结总结.md             # 完整技术文档
```

### 文档

- [工程完结总结](MY_ObstacleCraft/工程完结总结.md) — 完整避障逻辑、状态机思路、踩坑经验
- [工程实现目标及具体细节](MY_ObstacleCraft/工程实现目标及具体细节.md)

---

## License

MIT — 仅供学习参考，HMAC 密钥已内置仅作演示用途。
