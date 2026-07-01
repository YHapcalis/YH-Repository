# MY_OTA_GUI 工程计划表

**硬件配置：** F407ZGT6（仪表盘主控）+ F103C8T6（传感器/驱动节点）
**软件栈：** FreeRTOS (CMSIS-V2) + LVGL v9.3 (GUI-Guider 1.8.1 生成)
**通信：** CAN 500kbps + ISO-TP 15765
**目标：** OTA 可升级汽车仪表盘 + 遥控小车底盘

---

## 图例

| 符号 | 含义 |
|------|------|
| ✅ | 已完成 |
| 🟡 | 进行中 |
| ⬜ | 待开始 |

---

## Phase 0 — 当前基线

| # | 任务 | 文件 | 状态 |
|---|------|------|------|
| 0.1 | CubeMX 工程（FSMC/SPI/USART/ETH/GPIO） | `.ioc` | ✅ |
| 0.2 | NT35510 LCD 驱动（480×800） | `Core/Src/nt35510.c` | ✅ |
| 0.3 | W25Q128 SPI Flash 驱动 + 26 图验证 | `Core/Src/en25q128.c` | ✅ |
| 0.4 | 外部 SRAM 1MB — 线性格子 ≤256KB 安全 | 详见 PROGRESS.md | ✅ |
| 0.5 | FreeRTOS 任务骨架 (guiTask/touchTask/defaultTask/canRxTask) | `Core/Src/freertos.c` | ✅ |
| 0.6 | LVGL 9.3 源码 + lv_conf.h + GUI-Guider 生成 | `lvgl-9.3.0/` | ✅ |
| 0.7 | LVGL 显示端口 — PARTIAL 模式, 内部 50KB | `Core/Src/lv_port_disp.c` | ✅ |
| 0.8 | UART printf + 回显 | `freertos.c` defaultTask | ✅ |
| 0.9 | SPI Flash 批量烧录 (13批 6.1MB) | `scripts/main_batch.c` | ✅ |
| 0.10 | S: 路径文件系统 (lv_fs_spi_flash) | `Core/Src/lv_fs_spi_flash.c` | ✅ |
| 0.11 | 图片预加载到 SRAM (12 张小图 ~211KB) | `Core/Src/spiflash_images.c` | ✅ |
| 0.12 | **架构优化** — freertos.c → custom.c/hw_diag.c 重构 | 多项 | ✅ |
| 0.13 | **魔法数字常量化** — layout_defines.h | `Core/Inc/layout_defines.h` | ✅ |
| 0.14 | **转向灯实现** — 旋钮控制左右灯闪烁 | `Core/Src/custom.c` | ✅ |

---

## Phase 1 — CAN 通信基础设施

**目标：F407 ↔ C8T6 双向 CAN 通信**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 1.1 | F407 CubeMX 添加 CAN1（PA11/PA12, 500kbps） | `MY_OTA_GUI.ioc` | — | 新建 | ✅ |
| 1.2 | F407 CAN 接口层 canif.c/h | `BSP/Canif/canif.c` `.h` | 1.1 | `34Bootloader/canif/` | ✅ |
| 1.3 | F407 FreeRTOS canRxTask | `Core/Src/freertos.c` | 1.2 | 新建 | ✅ |
| 1.4 | C8T6 CubeMX 工程（CAN+USART2+I2C1+TIM1+TIM2+TIM4） | `MY_OTA_GUI_F103.ioc` | — | `01Can_Test/` | ✅ |
| 1.5 | C8T6 CAN 发送（ID=0x12, 1s 温湿度+旋钮+按键） | `Core/Src/app.c` | 1.4 | `27C8T6_Send/` | ✅ |
| 1.6 | F407 ICAN 接收温湿度 → 显示 TEMP tile | `BSP/Canif/canif.c` `custom.c` | 1.2, 2.6 | — | ✅ |
| **△ M1** | **C8T6 温湿度 → F407 LCD 显示** | | 全部 | | ✅ |

---

## Phase 2 — LVGL 仪表盘 UI

**目标：800×480 屏幕显示仪表盘（转速表 + 指示灯 + 日期时间）**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 状态 |
|---|------|---------|------|------|
| 2.1 | 识别触摸 IC (GT911) → 写驱动 | `Core/Src/touch/` | — | ✅ |
| 2.2 | 注册 LVGL 输入设备 + 独立触控 task | `Core/Src/touch/touch.c` | 2.1 | ✅ |
| 2.3 | GUI-Guider 生成 Home + Mode 屏 | `Core/Src/generated/` | — | ✅ |
| 2.4 | 实现 UI 交互 (A/F/C/E 切 Mode + 滑动手势) | `events_init.c` `custom.c` | 2.3 | ✅ |
| 2.5 | Home 屏小组件 (速度表/转向灯/电池等) | `setup_scr_home.c` | 2.3 | ✅ |
| 2.6 | Mode 屏 4 tile (NAV/TEMP/WEATHER/MUSIC) | `setup_scr_mode.c` | 2.3 | ✅ |
| 2.7 | 自模拟数据 (speed_meter_timer 等) | `custom.c` | 2.4 | ✅ |
| 2.8 | CMake 工程编译 + 烧录验证 | `CMakeLists.txt` | 2.1-2.7 | ✅ |
| **△ M2** | **仪表盘 UI 交互完整** | | 全部 | ✅ |

---

## Phase 3 — CAN → UI 数据通道

**目标：C8T6 传感器/控制数据 → F407 仪表盘 + 电机舵机控制**

### Phase 3A — 温湿度传输（已完成的先行阶段）

| # | 任务 | 涉及文件 | 状态 |
|---|------|---------|------|
| 3A.1 | C8T6 AHT20 采集 + CAN 发送（ID=0x12, 6字节精度帧） | `Core/Src/app.c` | ✅ |
| 3A.2 | F407 canRxTask 接收 + canif_parse_sensor 解析 | `BSP/Canif/canif.c` `freertos.c` | ✅ |
| 3A.3 | F407 TEMP tile 显示温湿度（mode_temp_update_cb） | `Core/Src/custom.c` | ✅ |
| 3A.4 | F407 旋钮 → 转向灯（home_turn_signal_cb） | `Core/Src/custom.c` | ✅ |
| **△ M3A** | **温湿度/旋钮/按键 → F407 显示完成** | | ✅ |

### Phase 3B — 单电机 + 单舵机驱动（F103 先行）

**背景：** 先在 F103 上跑通，再移植到 F407 做整车控制。

#### 硬件连接

```
F103 (MY_OTA_GUI_F103)
├── DRV8833 驱动一个电机（3V3/GND/IN1=PA0/IN2=PA1）
│   └── 直流电机 ×1（后轮驱动）
│   └── TIM2_CH1(PA0)=正转PWM, TIM2_CH2(PA1)=反转PWM
│   └── PWM 参数：5kHz, PSC=71, ARR=99
│   └── 驱动复用：`MY_ObstacleCraft/Core/Src/DRV8833.c`
│
├── SG-90 舵机 ×1（前轮转向）
│   └── TIM4_CH3(PB8)=50Hz PWM
│   └── PWM 参数：25Hz, PSC=719, ARR=1999
│   └── 舵机复用：`MY_ObstacleCraft/Core/Src/SG-90.c`
│
├── KEY1 = 前进（CAN 发 cmd=1）
├── KEY2 = 后退（CAN 发 cmd=2）
├── KEY3 = 刹车（CAN 发 cmd=3）
├── 旋钮 = 舵机偏转角度（-40°~+40°）
│
├── CAN ID=0x12 → 原有传感器数据（温湿度+旋钮+按键）
├── CAN ID=0x13 → 驾驶指令 [cmd, speed, 0...]
│   cmd: 0=空闲 1=前进 2=后退 3=刹车
└── CAN ID=0x14 → 心跳 [seq, 0...]（500ms）
```

#### 任务清单

| # | 任务 | 涉及文件 | 复用来源 | 状态 |
|---|------|---------|---------|------|
| 3B.1 | CubeMX 配 TIM2_CH1/CH2 PWM + TIM4_CH3 PWM | `MY_OTA_GUI_F103.ioc` | 已有 | ✅ |
| 3B.2 | 移植 DRV8833 单电机驱动 | `Core/Src/DRV8833.c/h` | `MY_ObstacleCraft` | ⬜ |
| 3B.3 | 移植 SG-90 舵机驱动 | `Core/Src/SG-90.c/h` | `MY_ObstacleCraft` | ⬜ |
| 3B.4 | app.c 集成：KEY1=前进 KEY2=后退 KEY3=刹车 | `Core/Src/app.c` | — | ⬜ |
| 3B.5 | app.c 集成：旋钮→SG90_SetAngle() | `Core/Src/app.c` | — | ⬜ |
| 3B.6 | CAN 发送驾驶指令（ID=0x13） | `Core/Src/app.c` | — | ⬜ |
| 3B.7 | CAN 发送心跳（ID=0x14） | `Core/Src/app.c` | — | ⬜ |
| 3B.8 | F407 接收 ID=0x13 指令并在屏幕显示 | `BSP/Canif/canif.c` `custom.c` | — | ⬜ |
| **△ M3B** | **单电机 + 单舵机 F103 跑通** | | | |

### Phase 3C — 双电机升级（F103 验证）

| # | 任务 | 涉及文件 | 说明 |
|---|------|---------|------|
| 3C.1 | DRV8833 双电机版（左右轮独立控制） | `Core/Src/DRV8833.c/h` | 改造为双实例，TIM2_CH1/CH2(左) TIM4_CH3/CH4(右) |
| 3C.2 | 差速转向逻辑（左轮/右轮不同转速实现转向） | `Core/Src/app.c` | 结合旋钮角度计算左右轮速差 |
| 3C.3 | ODO/TRIP/POWER 从电机脉冲推算 | `Core/Src/app.c` | 根据 PWM 占空比 × 运行时间估算里程 |
| **△ M3C** | **双电机差速驱动 F103 跑通** | | |

### Phase 3D — 升级到 F407 整车控制

| # | 任务 | 涉及文件 | 说明 |
|---|------|---------|------|
| 3D.1 | F407 CubeMX 配电机/舵机 TIM（需空闲 PWM 引脚） | `MY_OTA_GUI.ioc` | 找 F407 可用 TIM 通道 |
| 3D.2 | DRV8833 + SG-90 移植到 F407 | `BSP/Driver/` | 从 F103 移植驱动，改 TIM 句柄 |
| 3D.3 | C8T6 退化为纯操作端（只发指令，不直接驱动物理硬件） | `Core/Src/app.c` | 指令通过 CAN→F407→电机/舵机 |
| 3D.4 | F407 集成电机控制 + 仪表盘显示合一 | `BSP/Canif/can_handle.c` | 单芯片搞定一切 |
| **△ M3D** | **F407 整车控制 + 仪表盘** | | |

---

## Phase 4 — Flash 参数管理层

**目标：内部 Flash 读写 ota_flag，实现"标记→复位→跳转"机制**

**⚠ F407 是 sector 架构，与 F103 的 page 架构不同**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 4.1 | 定义 F407 Flash 分区 | `BSP/Flash/inter_flash_cfg.h` | — | `08Ymodem_Bootloader/inter_flashif/inter_flash_cfg.h` | ⬜ |
| — | 参数区=Sector3 0x0800C000, APP=0x08010000 | | | | |
| 4.2 | F4 sector 擦写 + 读 | `BSP/Flash/inter_flashif.c` `.h` | 4.1 | `08Ymodem_Bootloader/inter_flashif/` | ⬜ |
| — | **重点：** addr→sector 转换，FLASH_TYPEERASE_SECTORS | | | | |
| 4.3 | 参数字段读写 | `BSP/Flash/inter_flash_cfg.c` `.h` | 4.2 | `34Bootloader/Core/Src/inter_flash_cfg.c` | ⬜ |
| — | magic(0xAABBCCDD)+ota_flag+checksum | | | | |
| 4.4 | 上电验证 | `main.c` 临时测试 | 4.3 | — | ⬜ |
| — | 写1→复位→printf 读回 | | | | |
| **△ M4** | **ota_flag 写入/断电保持/读出** | | 全部 | | |

---

## Phase 5 — OTA 协议层

**目标：CAN ISO-TP 接收 OTA 数据并写入内部 Flash**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 5.1 | 移植 ISO-TP 协议栈 | `BSP/IsoTp/isotp.c/h` + `defines.h` + `config.h` | M1 | `34Bootloader/iso_tpif/` | ⬜ |
| — | isotp_user_get_ms()→HAL_GetTick() | | | | |
| — | isotp_user_send_can()→调CAN1_Send_Msg() | | | | |
| 5.2 | OTA 服务端 iso_tp_cfg | `BSP/IsoTp/iso_tp_cfg.c` `.h` | 5.1, M4 | `34Bootloader/Core/Src/iso_tp_cfg.c` | ⬜ |
| — | [0xBF][4B offset][4B size][N data] + [BE,AD,BE,EF,02]结束 | | | | |
| 5.3 | OTA 任务集成 | `Core/Src/freertos.c` — StartOTATask() | 5.2 | — | ⬜ |
| — | otaTask 中调 iso_tp_server() | | | | |
| **△ M5** | **CAN 接收 OTA 数据并写入 Flash** | | 全部 | | |

---

## Phase 6 — Bootloader 工程

**目标：独立 Bootloader → 读 ota_flag → 跳转 APP / 进入 OTA**

**注意：这是独立工程，与主工程不同的 CubeMX 配置**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 6.1 | CubeMX 新建工程 | `Bootloader/` | — | `34Bootloader/bootloader.ioc` | ⬜ |
| — | 仅 CAN+USART1，链接脚本 64KB@0x08000000 | | | | |
| 6.2 | 移植 boot_manager | `Bootloader/Core/Src/boot_manager.c` | 6.1 | `34Bootloader/Core/Src/boot_manager.c` | ⬜ |
| — | boot_check_stack2jump_app(0x08010000) | | | | |
| 6.3 | 复制 inter_flashif + inter_flash_cfg | `Bootloader/BSP/Flash/` | 6.1 | 同 P4 | ⬜ |
| 6.4 | 复制 ISO-TP + iso_tp_cfg | `Bootloader/BSP/IsoTp/` | 6.1 | 同 P5 | ⬜ |
| 6.5 | Bootloader 主逻辑 | `Bootloader/Core/Src/main.c` | 6.2~6.4 | `34Bootloader/Core/Src/main.c` | ⬜ |
| — | 读flag→OTA模式/跳APP | | | | |
| 6.6 | 烧写验证 | — | 6.5 | — | ⬜ |
| — | APP 区空→停留 Bootloader；烧 APP→自动跳转 | | | | |
| **△ M6** | **Bootloader 判 flag → 跳转 APP** | | 全部 | | |

---

## Phase 7 — 全链路 OTA 联调

**目标：C8T6 通过 CAN 给 F407 做 OTA 升级**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 7.1 | C8T6 OTA 发送端 | `C8T6_Sensor/ota_sender.c` | M5, M6 | `30OTA_Bin_Test/` 看 bin 格式 | ⬜ |
| — | ①发触发帧 [BE,AD,BE,EF,01]@0xB3 | | | `27C8T6_Send/` 参考 CAN 发送 | |
| — | ②收 ACK→ISO-TP 发数据@0xB2 | | | `35C8T6_Send_Faker/` 参考 | |
| — | ③发结束帧 [BE,AD,BE,EF,02] | | | | |
| 7.2 | F407 APP 端 OTA 触发 | `can_handle.c` | 7.1, M5 | `32Car_APP/BSP/Canif/can_handle.c` | ⬜ |
| — | 收到 [BE,AD,BE,EF,01] → inter_flash_cfg_set(1) → 复位 | | | | |
| 7.3 | 全链路闭环测试 | — | 7.1, 7.2 | — | ⬜ |
| — | APP 运行→C8T6 触发→复位→Bootloader 收→烧写→跳新 APP | | | | |
| 7.4 | 容错测试 | — | 7.3 | — | ⬜ |
| — | 中断续传 / CRC 失败不跳转 | | | | |
| **△ M7** | **CAN OTA 全链路跑通** | | 全部 | | |

---

## Phase 8 — 收尾

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 8.1 | 双区备份（可选） | SPI Flash 存固件 | M7 | — | ⬜ |
| — | 先写 SPI Flash→校验→复制内部 Flash | | | | |
| 8.2 | 代码清理 + 注释 | 全部 | — | — | ⬜ |
| 8.3 | 文档整理 | `docs/` | — | — | ⬜ |

---

## 依赖关系图

```text
P1 (CAN基础) ─── P3A (温湿度) ───────────────────────────────────
                         │
                    P3B (单电机单舵机 F103) ─── P3C (双电机 F103)
                                                     │
                                               P3D (升级到 F407)
                                                     │
P2 (LVGL UI) ─────────────────────────────────────────┤
                                                      │
                                                 M3 完整仪表盘+底盘
                                                      │
P4 (Flash参数) ─── P5 (OTA协议) ─── P7 (全链路联调) ─── M7 最终
                               │           ▲
                               └── P6 (Bootloader) ──┘

独立项: P2 ↔ P4 (可并行)
```

---

## CAN 帧格式总表

| ID | 方向 | 周期 | Byte0 | Byte1 | Byte2 | Byte3 | Byte4 | Byte5 | Byte6-7 |
|----|------|------|-------|-------|-------|-------|-------|-------|---------|
| 0x12 | F103→F407 | 1000ms | 温度整数+40 | 温度小数×10 | 湿度整数 | 湿度小数×10 | 旋钮值 | 按键事件 | 保留 |
| 0x13 | F103→F407 | 事件/1s | cmd | speed% | 0 | 0 | 0 | 0 | 0 |
| 0x14 | F103→F407 | 500ms | seq | 0 | 0 | 0 | 0 | 0 | 0 |

### 帧 0x13 指令表

| cmd | 含义 | speed 含义 |
|-----|------|-----------|
| 0 | 空闲 | 0 |
| 1 | 前进 | 0-100（目标速度） |
| 2 | 后退 | 0-100（目标速度） |
| 3 | 刹车 | 0（立即停止） |

---

## 备注

1. **P2（LVGL UI）和 P4（Flash 参数）互相独立**，可以并行推进
2. **P5 调试可不依赖 P4**：先用 printf 代替 Flash 写入验证协议完整性
3. **P6（Bootloader）是独立工程**，与主工程共用 BSP 代码但 CMake target 不同
4. 触摸驱动 (P2.1) 已完成 — GT911 芯片, 独立 FreeRTOS task 100Hz 轮询
5. 缺失的第二个 C8T6 的职能由 F407 `canSimTask` 自模拟替代，不走 CAN 总线，接真实 ECU 时 `#if 0` 关闭
6. 外部 SRAM 可用 ≤256KB 线性格子, 大图走 S: 路径流式解码
7. Mode 屏 tile 背景最终用纯 LVGL 色 (非 S: 图片) — 切换 Tile 才流畅
8. **Phase 3B~3D 先基于 F103 验证，再移植到 F407**
9. **F103 电机/舵机引脚已由 CubeMX 配置完毕**
