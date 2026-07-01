# CAN 通信配置记录

> 最后更新：2026-06-29
> 状态：✅ F103 ↔ F407 双向通信正常

---

## 一、硬件架构

```
F103 (MY_OTA_GUI_F103)                    F407 (MY_OTA_GUI)
────────────────────────                   ────────────────────────
主控: STM32F103C8T6                        主控: STM32F407ZGT6
收发器: TJA1050 独立模块                   收发器: TJA1040 板载 (U8)
供电: 5V 通过 USB                          供电: 板载 5V → 3.3V
                                           CAN 总线引出: P7 接口 (CANH/CANL)
                                           终端电阻: R18 120Ω 板载 (CANH↔CANL)

F103 PA11(CAN_RX) ─→ TJA1050 RXD
F103 PA12(CAN_TX) ─→ TJA1050 TXD
TJA1050 CANH ── 杜邦线 ──→ F407 P7 pin1 (CANH)
TJA1050 CANL ── 杜邦线 ──→ F407 P7 pin2 (CANL)
                    GND ── 杜邦线 ──→ GND (共地)
```

---

## 二、引脚配置 (关键！F103 ≠ F407)

### F103 (STM32F103C8T6)

| 信号 | 引脚 | GPIO 模式 | 说明 |
|------|------|----------|------|
| CAN_RX | PA11 | `GPIO_MODE_INPUT` | F103 的 CAN RX 不需要 AF 配置 |
| CAN_TX | PA12 | `GPIO_MODE_AF_PP` | F103 的 CAN TX 需要 AF 配置 |

```c
// F103 — CAN MSP Init (正确配置)
GPIO_InitStruct.Pin = GPIO_PIN_11;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;       // RX = INPUT
GPIO_InitStruct.Pull = GPIO_NOPULL;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

GPIO_InitStruct.Pin = GPIO_PIN_12;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;       // TX = AF_PP
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

### F407 (STM32F407ZGT6) ⚠️ 与 F103 不同

| 信号 | 引脚 | GPIO 模式 | 说明 |
|------|------|----------|------|
| CAN_RX | PA11 | `GPIO_MODE_AF_PP` | **F407 的 RX 也必须 AF，不是 INPUT！** |
| CAN_TX | PA12 | `GPIO_MODE_AF_PP` | AF9 = CAN1 |
| AF 选择 | — | `GPIO_AF9_CAN1` | 两个引脚共用同一个 AF |

```c
// F407 — CAN MSP Init (正确配置，与 F103 不同！)
GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;  // 一起配置
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;           // RX 和 TX 都是 AF_PP
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;        // AF9 = CAN1
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);           // 一次调用
```

> ⚠️ **踩坑记录（2026-06-29）：** F407 的 CAN_RX 必须配置为 `GPIO_MODE_AF_PP`，不能像 F103 那样配成 `GPIO_MODE_INPUT`。 
> F103 的 CAN 外设会自动接管 RX 引脚，F407 必须通过 AF 路由才能将引脚连接到 CAN 外设。
> 此 Bug 耗费 2 小时定位，修复后通信立即恢复。

---

## 三、时序参数（500kbps）

### F103
```
APB1 = 36MHz
Prescaler = 12
tq = 12 / 36MHz = 0.333μs
SyncSeg = 1TQ, TS1 = 4TQ, TS2 = 1TQ
Bit Time = (1 + 4 + 1) × 0.333 = 2.0μs → 500kbps
```

### F407
```
APB1 = 42MHz
Prescaler = 6
tq = 6 / 42MHz = 0.143μs
SyncSeg = 1TQ, TS1 = 11TQ, TS2 = 2TQ
Bit Time = (1 + 11 + 2) × 0.143 = 2.0μs → 500kbps
```

两边 tq 不同，但总 bit time 都是 2.0μs，通信匹配。

---

## 四、过滤器配置（全接收模式）

两边一样，不过滤任何 ID，软件层区分：

```c
sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
sFilterConfig.FilterIdHigh = 0x0000;
sFilterConfig.FilterIdLow = 0x0000;
sFilterConfig.FilterMaskIdHigh = 0x0000;   // Mask = 0: 全部通过
sFilterConfig.FilterMaskIdLow = 0x0000;
sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;
```

> F407 注意：`SlaveStartFilterBank = 14`（CAN2 未使用时的标准值）。

---

## 五、CAN 帧格式

| ID | 方向 | 周期 | Byte0 | Byte1 | Byte2 | Byte3 | Byte4 | Byte5 |
|----|------|------|-------|-------|-------|-------|-------|-------|
| 0x12 | F103→F407 | 1000ms | 温度整数+40 | 温度小数×10 | 湿度整数 | 湿度小数×10 | 旋钮值 | 按键事件 |
| 0x13 | F103→F407 | 事件 | cmd | speed% | 0 | 0 | 0 | 0 |
| 0x14 | F103→F407 | 500ms | seq | 0 | 0 | 0 | 0 | 0 |

### 帧 0x12 说明
带 0.1 精度传输，F103 发送，F407 解析：
```c
// F103 发送
float t_frac = s_temp - (int16_t)s_temp;
tx[0] = (uint8_t)((int16_t)s_temp + 40);
tx[1] = (uint8_t)(t_frac * 10.0f + 0.5f);
tx[2] = (uint8_t)((int16_t)s_hum);
tx[3] = (uint8_t)(h_frac * 10.0f + 0.5f);
tx[4] = (uint8_t)s_knob;
tx[5] = s_key_last;
CAN1_Send_Msg(CAN_ID_SENSOR, tx, 6);

// F407 解析
int8_t temp_int = (int8_t)(buf[0] - 40);
float  temp_dec = (float)buf[1] / 10.0f;
temp = (float)temp_int + temp_dec;
hum  = (float)buf[2] + (float)buf[3] / 10.0f;
knob = buf[4];
key  = buf[5];
```

### 帧 0x13 指令表
| cmd | 含义 | speed 含义 |
|-----|------|-----------|
| 0 | 空闲 | 0 |
| 1 | 前进 | 0-100（目标速度） |
| 2 | 后退 | 0-100（目标速度） |
| 3 | 刹车 | 0（立即停止） |

---

## 六、代码架构

```
F103 (裸机轮询)                          F407 (FreeRTOS)
────────────────────                     ────────────────────────
main → App_Init / App_Task               main → MX_*_Init → osKernelStart
  app.c (状态机)                            freertos.c (RTOS 任务)
    ├─ AHT20 采集 (2s)                         ├─ StartGUITask ── LVGL
    ├─ CAN 发送 (1s, ID=0x12)                │    └─ canif_init()
    ├─ CAN 发送指令 (ID=0x13)                    └─ canif_init()
    ├─ CAN 发送心跳 (ID=0x14)
    ├─ OLED 刷新 (200ms)
    └─ 按键/旋钮扫描

  canif.c (CAN 封装)                        canif.c (CAN 封装)
    ├─ canif_init()                           ├─ canif_init()
    ├─ CAN1_Send_Msg()                        ├─ CAN1_Recv_Msg()
    └─ CAN1_Recv_Msg()                        └─ canif_parse_sensor()

  DRV8833.c / SG-90.c ─── 已迁移到 F407      DRV8833.c (TIM8_CH1/CH2, PC6/PC7)
                                              SG-90.c  (TIM11_CH1, PF7)
```

---

## 七、常见错误

### 1. F407 CAN_RX 配置错误
- ❌ `GPIO_MODE_INPUT`（F103 的做法）
- ✅ `GPIO_MODE_AF_PP` + `GPIO_AF9_CAN1`

### 2. CubeMX 重新生成覆盖引脚
- 问题：CubeMX 重新生成时可能把 CAN1_RX 改成 PB8
- 修复：每次 CubeMX 生成后检查 `HAL_CAN_MspInit`，确保 PA11/PA12 正确

### 3. 过滤器配置
- 如果 Mask 不为 0，可能把需要的 ID 滤掉
- 调试阶段：Mask = 0（全部通过）

### 4. 终端电阻
- F407 板载 R18 = 120Ω（焊在 CANH↔CANL 之间）
- F103 TJA1050 模块需要跳线帽或外接 120Ω
- 两个节点都接 120Ω = 总线等效 60Ω（标准）

### 5. 物理层故障排查
- `[CAN] ESR=0x00000000` 但没有收到数据 = CAN 控制器健康但 RX 引脚无信号
- 原因：PIN 配置错、收发器没供电、CANH/CANL 接反或内断

---

## 八、诊断命令

| 症状 | 排查方向 |
|------|---------|
| `[CAN] Send FAIL` | F103 发送失败，检查总线是否空闲、F407 是否在线 |
| F407 ESR 全零无数据 | CAN RX 引脚配置错误或收发器未通电 |
| ESR 中 TEC/REC 持续增长 | 总线错误，检查终端电阻和接线 |
| `[CAN] Start FAIL` | HAL_CAN_Start 失败，检查时钟和引脚配置 |
