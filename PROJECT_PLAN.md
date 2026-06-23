# MY_OTA_GUI 工程计划表

**硬件配置：** F407ZGT6（仪表盘主控）+ F103C8T6（传感器节点）
**软件栈：** FreeRTOS (CMSIS-V2) + LVGL v9.5
**通信：** CAN 500kbps + ISO-TP 15765
**目标：** OTA 可升级汽车仪表盘

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
| 0.3 | EN25Q128 SPI Flash 驱动 + 全功能验证 | `Core/Src/en25q128.c` | ✅ |
| 0.4 | 外部 SRAM 验证（1MB @ 0x68000000） | `main.c` Test_ExtSRAM() | ✅ |
| 0.5 | FreeRTOS 任务骨架（guiTask/otaTask/defaultTask） | `Core/Src/freertos.c` | ✅ |
| 0.6 | LVGL 9.5 源码 + lv_conf.h | `lvgl-9.5.0源码/` | ✅ |
| 0.7 | LVGL 显示端口 | `Core/Src/lv_port_disp.c` | ✅ |
| 0.8 | UART printf + 回显 | `freertos.c` defaultTask | ✅ |

---

## Phase 1 — CAN 通信基础设施

**目标：F407 ↔ C8T6 双向 CAN 通信**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 1.1 | CubeMX 添加 CAN1（PD0/PD1, 500kbps） | `MY_OTA_GUI.ioc` | — | 新建 | ⬜ |
| 1.2 | 写 CAN 接口层 canif.c/h | `BSP/Canif/canif.c` `.h` | 1.1 | `34Bootloader/canif/` | ⬜ |
| — | canif_init() / CAN1_Send_Msg() / CAN1_Recv_Msg() | | | | |
| 1.3 | FreeRTOS 创建 canRxTask | `Core/Src/freertos.c` | 1.2 | 新建 | ⬜ |
| — | 轮询 CAN FIFO → printf | | | | |
| 1.4 | C8T6 CubeMX 工程（CAN+USART1） | `C8T6_Sensor/` | — | `01Can_Test/` `.ioc` | ⬜ |
| 1.5 | C8T6 CAN 发送（ID=0x12, 1s） | `C8T6_Sensor/main.c` | 1.4 | `27C8T6_Send/` | ⬜ |
| **△ M1** | **C8T6 发 → F407 收** | | 全部 | | |

---

## Phase 2 — LVGL 仪表盘 UI

**目标：800×480 屏幕显示仪表盘（转速表 + 指示灯 + 日期时间）**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 2.1 | 识别触摸 IC → 写触摸驱动 | `BSP/Touch/touch.c` `.h` | — | 按屏资料新建 | ⬜ |
| — | *可选：暂时不做也不影响，按键可替代* | | | | |
| 2.2 | 注册 LVGL 输入设备 | `Core/Src/lv_port_indev.c` | 2.1 | `11中景园硬件SPI带触摸标准库例程/` | ⬜ |
| 2.3 | 创建仪表盘 UI 布局 | `Core/Src/ui.c` `.h` | — | `25Lvgl_Dashboard _Flashok/` (v8.3 → v9.5) | ⬜ |
| — | lv_scale 做刻度盘，指针 lv_image_set_rotation(deg*100) | | | | |
| 2.4 | 实现 UI 更新接口 | `ui.c` | 2.3 | `25Lvgl_Dashboard _Flashok/` 中 ui.c 的 lv_Label 等 | ⬜ |
| — | ui_set_rpm_angle() / ui_set_light_color() / ui_set_date_time() | | | | |
| 2.5 | 自模拟验证 UI | `freertos.c` | 2.4 | `25Lvgl_Dashboard _Flashok/Core/Src/main.c` need_angle 循环 | ⬜ |
| **△ M2** | **屏幕显示仪表盘，自模拟数据刷新** | | 全部 | | |

---

## Phase 3 — CAN → UI 数据通道

**目标：C8T6 发传感器数据 + F407 自模拟 → 驱动仪表盘**

> 例程根目录：`E:\ST\STM32\MY_workspace\《OTA汽车仪表盘项目》\资料\uint3code\uint3code\`

| # | 任务 | 涉及文件 | 依赖 | 例程参考 | 状态 |
|---|------|---------|------|---------|------|
| 3.1 | 移植 CAN→UI 解析层 | `BSP/Canif/can_handle.c` `.h` | M1, M2 | `32Car_APP/BSP/Canif/` | ⬜ |
| — | Can_Handle() 解析 ID+Data → ui_Typedef | | | | |
| — | canRxTask 中调用 | | | | |
| 3.2 | C8T6 接 DHT11 并 CAN 发送 | `C8T6_Sensor/` | 1.4 | `04Can_DHT11_OLED/DHT11/` | ⬜ |
| — | ID=0x12, Data[湿度, 温度] | | | | |
| 3.3 | F407 接收 DHT11 → 显示 | `can_handle.c` `ui.c` | 3.1, 3.2 | `04Can_DHT11_OLED/Core/Src/main.c` | ⬜ |
| 3.4 | F407 自模拟任务 canSimTask | `Core/Src/freertos.c` | M2 | 新建（接真实 ECU 时 #if 0 关闭） | ⬜ |
| — | 500ms 自增转速+指示灯，调 ui_set_xxx() | | | | |
| **△ M3** | **C8T6 温湿度→F407 显示 + 自模拟表盘** | | 全部 | | |

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
P1 (CAN基础) ───────────────────────────────────────────────────────
                                                                   │
P2 (LVGL UI) ────── P3 (CAN→UI通道) ───────────────────────────────┤
                                                                   │
                                                              M3 完整仪表盘
                                                                   │
P4 (Flash参数) ─── P5 (OTA协议) ─── P7 (全链路联调) ─── M7 最终
                                │           ▲
                                └── P6 (Bootloader) ──┘

独立项: P2 ↔ P4 (可并行)
```

---

## 备注

1. **P2（LVGL UI）和 P4（Flash 参数）互相独立**，可以并行推进
2. **P5 调试可不依赖 P4**：先用 printf 代替 Flash 写入验证协议完整性
3. **P6（Bootloader）是独立工程**，与主工程共用 BSP 代码但 CMake target 不同
4. 如果暂时不做触摸，P2.1~2.2 跳过即可，`Can_Handle()` 收 CAN 数据驱动 UI 不需要触摸
5. 缺失的第二个 C8T6 的职能由 F407 `canSimTask` 自模拟替代，不走 CAN 总线，接真实 ECU 时 `#if 0` 关闭
