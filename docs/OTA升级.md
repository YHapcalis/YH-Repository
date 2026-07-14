# OTA 升级 — 传输模式详解

> **工程**：MY_OTA_GUI (STM32F407ZGT6 + ESP8266 + F103C8T6)  
> **更新**：2026-07-13  
> **范围**：WiFi 定向推送 / 广播方案 / CAN ISO-TP

---

## 一、WiFi 定向推送（已验证）

### 1.1 架构

```
PC ──TCP:8080──► ESP8266 ──USART3/PB10/PB11──► F407 ──SPI1──► W25Q128 (0xE00000)
                                                          │
                                                           └──► NVIC_SystemReset()
                                                                    │
                                                          Bootloader 读取 SPI Flash
                                                                    │
                                                            恢复 → 签名验证 → 跳转 APP
```

### 1.2 命令格式

| 方向 | 格式 | 示例 | 说明 |
|:----:|------|------|------|
| PC→板 | `OTA:<size>\r\n` | `OTA:471276\r\n` | 起始命令，size=固件字节数 |
| PC→板 | `<binary data>` | 128B × N | 固件二进制数据块 |
| PC→板 | `WHO\r\n` | `WHO\r\n` | UID 握手查询 |
| 板→PC | `ID:<UID>\r\n` | `ID:0039003A3032...\r\n` | MCU 96-bit 唯一 ID |

### 1.3 传输参数

| 参数 | 值 | 说明 |
|:-----|:---:|------|
| 传输协议 | TCP | 可靠传输，自动重传 |
| 端口 | 8080 | ESP8266 TCP Server 监听 |
| 块大小 | 128B | ESP8266 UART 缓冲安全值 |
| 块间延时 | 20ms | SPI Flash 页编程 (~5ms) 余量 |
| 固件大小 | ≤1MB | 当前 471KB (55% of 832KB) |
| 速率 | ~6.1 KB/s | 瓶颈：USART3 @115200baud |
| 总耗时 | ~75s/470KB | 实测值 |

### 1.4 +IPD 帧格式

ESP8266 接收到 TCP 数据后，通过 USART3 以 +IPD 帧格式转发给 MCU：

```
+IPD,<id>,<len>:<data>\r\n

示例:
+IPD,0,7:OTA:471276\r\n       ← 起始命令 (len=7)
+IPD,0,128:<128B二进制数据>\r\n ← 固件块 (len=128)
```

MCU 端 USART3 中断逐字节接收，存入 `strEsp8266_Fram_Record`，遇到 `\n` 标记帧完成。

### 1.5 关键代码路径

| 文件 | 函数 | 职责 |
|------|------|------|
| `stm32f4xx_it.c` | `USART3_IRQHandler()` | 逐字节接收 ESP8266 数据 |
| `freertos.c` | `StartWiFiTask()` | +IPD 解析 + OTA 状态机 |
| `freertos.c` | `StartWiFiTask()` OTA 完成块 | 写备份头 + 设 flag + 复位 |
| `Bootloader/Src/main.c` | `main()` SPI Flash 检测 | 检测 SPI_BAK_MAGIC 恢复 |
| `en25q128.c` | `EN25Q128_RestoreFirmware()` | 从 SPI Flash 恢复到内部 Flash |

### 1.6 验证结果

```
[DIRECTED] Connected!
[OTA] Sent: OTA:471276
[OTA] 100% (471276/471276 bytes, 6.1 KB/s)
[OTA] Done! 471276 bytes in 75.3s (6.1 KB/s)
[OTA] Board should reboot into bootloader now.

--- 15s later ---
PC=0x08049c94    ← APP 代码区
uwTick=0x2e58    ← 系统心跳正常
STATUS: PASS     ← 全链路通过
```

---

## 二、WiFi 广播推送（未实现）

### 2.1 目标

```
PC ──UDP:8081──► 所有 ESP8266 同时收到 ──► 各板独立下载固件
```

不依赖 PC 逐个连接，耗时固定（~75s），与板数无关。

### 2.2 预设协议

```c
// PC → UDP 广播 (255.255.255.255:8081):
"OTA:<size>:<server_ip>:<port>\r\n"

// 示例:
"OTA:471276:192.168.0.219:8765\r\n"
//                     ↑PC下载服务器IP  ↑端口

// 各板解析后:
// 1. 关闭 UDP socket (AT+CIPCLOSE)
// 2. 连接 PC 下载服务器 (AT+CIPSTART="TCP","192.168.0.219",8765)
// 3. 接收固件数据
// 4. 写入 SPI Flash → 复位 → Bootloader 恢复
```

### 2.3 失败方案对照表

| # | 方案 | AT 命令 | 结果 | 失败原因 |
|:-:|:----|---------|:----:|:---------|
| 1 | MUX + ID 5 | `AT+CIPSTART=5,"UDP"...` | **ID ERROR** | MUX 模式最大 ID=4，ID 5 不存在 |
| 2 | MUX + ID 4 | `AT+CIPSTART=4,"UDP"...` | **ALREADY CONNECTED** | `AT+CIPSERVER=1` 为 TCP Server 预留了 ID 4 |
| 3 | MUX + ID 3 | `AT+CIPSTART=3,"UDP"...` | **CONNECT FAIL** | MUX+Server 模式下禁止混合 UDP/TCP 协议 |
| 4 | 单模 + 本地端口 | `AT+CIPMUX=0` → `AT+CIPSTART="UDP","255.255.255.255",8081,8081` | **ERROR** | 单模式 UDP 不支持绑定本地端口 |
| 5 | 单模 + mode=2 | `AT+CIPSTART="UDP","255.255.255.255",8081,8081,2` | **ERROR** | `mode=2` 参数仅 MUX 模式有效 |
| 6 | 单模 + 无远程 | `AT+CIPSTART="UDP","0.0.0.0",0,8081` | **ERROR** | AT 固件 UDP 实现需要有效远程地址 |

### 2.4 根因分析

**ESP8266 AT 固件的核心限制**是一个单线程的 AT 命令解释器，不是完整的多任务系统：

```
                    ESP8266 AT Firmware
┌─────────────────────────────────────────────┐
│         单线程 AT 命令解释器                  │
│  发 AT → 收响应 → 发 AT → 收响应 → ...      │
│                                             │
│  UDP 实现方式: "轻量 TCP"                    │
│  ┌──────────────────────────────────────┐   │
│  │ UDP socket 需要先 "连接" 到远程地址   │   │
│  │ 不支持真正无连接的 UDP 接收(如广播)    │   │
│  │ 不支持多路复用不同协议(TCP+UDP 共存)  │   │
│  └──────────────────────────────────────┘   │
│                                             │
│  TCP Server 开启后:                         │
│  ┌──────────────────────────────────────┐   │
│  │ 预留 MUX ID 0-4 给 TCP 连接          │   │
│  │ 不允许创建 UDP socket                │   │
│  │ 不允许协议混用                       │   │
│  └──────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
```

**四层受限关系：**

```
                     ┌─────────────────┐
                     │  应用层需求      │
                     │  UDP 广播接收    │
                     ├─────────────────┤
                     │  TCP Server 共存 │
                     ├─────────────────┤
                     │  AT 固件能力    │
                     ├─────────────────┤
                     │  ESP8266 SDK    │
                     │  (RTOS SDK)     │
                     └─────────────────┘
                           ↕ 逐层受限

第 1 层 — ESP8266 SDK 限制:
  lwIP 协议栈支持 UDP 广播接收
  → 但 AT 固件未暴露此能力

第 2 层 — AT 固件架构限制:
  单线程命令循环, 不支持后台监听
  UDP 被实现为"连接导向协议"

第 3 层 — AT CIPSERVER 限制:
  TCP Server 启用后独占 MUX ID 池
  不开放混合协议支持

第 4 层 — UART 带宽限制:
  115200baud → ~11.5KB/s 理论峰值
  → +IPD 帧包装额外开销 → ~6KB/s 实测
  → N 块板同时广播时 ESP8266 缓冲溢出
```

### 2.5 可用替代方案

| 方案 | 原理 | 实现量 | 推荐度 |
|:----|:-----|:------:|:------:|
| **顺序 TCP 推送** | PC 逐一连接各板推送 | 现有功能 | ⭐⭐⭐ **现成** |
| **HTTP 轮询** | 板子定期 GET 版本文件 | 小 | ⭐⭐⭐ **推荐** |
| **MQTT 订阅** | Broker 发布, 板子订阅 OTA 主题 | 中 | ⭐⭐ |
| **替换 ESP8266 固件** | 放弃 AT, 用 ESP-OPEN-RTOS 自写 UDP | 大 | ⭐ |
| **换 ESP32** | 原生 UDP 广播支持 | PCB 改版 | ⭐⭐⭐ |

---

### 2.6 替代方案一：MQTT 订阅模式（推荐）

#### 原理

```
PC (Publisher) ──发布──► MQTT Broker ──转发──► 板1 (Subscriber)
                                        ──转发──► 板2 (Subscriber)
                                        ──转发──► ... (所有板同时收到)
```

一条 MQTT 消息 → **所有板子同时收到** → 各板独立下载固件。耗时固定，与板数无关。

#### 架构

```
┌─────────────────────────────────────────────┐
│  PC (同一台机器跑三个服务)                    │
│                                              │
│  1. mosquitto Broker (MQTT 消息中心)         │
│     安装: choco install mosquitto            │
│           或官网下载 exe 双击安装              │
│     端口: 1883 (TCP)                        │
│     状态: Windows 后台服务, 开机自启          │
│                                              │
│  2. HTTP 文件服务器 (固件下载)                │
│     命令: python -m http.server 8080         │
│     目录: build/Debug/                       │
│                                              │
│  3. OTA 通知发布脚本 (跑一次即退出)            │
│     脚本: scripts/mqtt_ota_pub.py            │
│     命令: python mqtt_ota_pub.py             │
└─────────────────────────────────────────────┘
       │                    │
       │ MQTT :1883          │ HTTP :8080
       ▼                    ▼
┌──────────────┐    ┌──────────────────┐
│ MQTT Broker   │    │ HTTP 文件服务器   │
│ mosquitto     │    │ http.server      │
│ 转发 OTA 通知  │    │ 提供固件下载      │
└──────┬───────┘    └──────────────────┘
       │
       │ 订阅: ota/notify
       ▼
┌──────────────────────────────────────────────┐
│  ESP8266 (AT+MQTTSUB) → F407 MCU 解析        │
│                                              │
│  +MQTTSUBRECV:0,"ota/notify",<len>:<payload> │
│        ↓                                     │
│  JSON 解析: {"v":3,"size":471276,            │
│              "url":"http://192.168.0.219:8080 │
│                    /MY_OTA_GUI.bin"}          │
│        ↓                                     │
│  AT+HTTPGET url → 下载固件                    │
│        ↓                                     │
│  写 SPI Flash → 备份头 → 复位                 │
└──────────────────────────────────────────────┘
```

#### PC 端部署

**1. 安装 mosquitto**

```powershell
# 方式 A: chocolatey (推荐)
choco install mosquitto

# 方式 B: 官网下载
# https://mosquitto.org/download/
# 安装后自动注册为 Windows 服务
```

**2. 启动 HTTP 文件服务器**

```powershell
cd build/Debug
python -m http.server 8080
```

**3. 发布 OTA 通知脚本**

```python
#!/usr/bin/env python3
"""scripts/mqtt_ota_pub.py — 发一条 MQTT 消息, 所有板子同时收到"""
import paho.mqtt.client as mqtt
import json, os, socket

FW_PATH = "build/Debug/MY_OTA_GUI.bin"
HTTP_PORT = 8080

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.connect(("8.8.8.8", 80))
my_ip = s.getsockname()[0] ; s.close()

fw_size = os.path.getsize(FW_PATH)
payload = json.dumps({
    "v": 3,
    "size": fw_size,
    "url": f"http://{my_ip}:{HTTP_PORT}/MY_OTA_GUI.bin"
})

client = mqtt.Client()
client.connect("127.0.0.1", 1883)  # 连本机 mosquitto
client.publish("ota/notify", payload, qos=1)
print(f"Published: {payload}")
client.disconnect()
```

#### F407 固件集成

**ESP8266 MQTT AT 命令序列：**

```
AT+MQTTUSERCFG=0,1,"f407_ota","","",0,0,""
  → OK
AT+MQTTCONN=0,"192.168.0.219",1883,0
  → OK
AT+MQTTSUB=0,"ota/notify",1
  → OK

等待消息:
+MQTTSUBRECV:0,"ota/notify",<len>:<json payload>
```

**MCU 端流程：**

```
WiFi 任务主循环:
  ├─ 现有 +IPD 解析 (定向 OTA 兼容)
  ├─ 新增 +MQTTSUBRECV 解析:
  │     ↓
  │   提取 version → 对比当前版本号
  │   → 版本一致? 忽略
  │   → 版本更新?
  │       AT+HTTPGET url → 依次接收
  │       → 每收到一块 → 写 SPI Flash
  │       → 下载完成 → 写备份头 → 复位
  └─ 30s 保活检测
```

#### FAQ

| 问题 | 回答 |
|:-----|:------|
| **mosquitto 必须在 C 盘装服务？** | 也可以解压绿色版，`mosquitto.exe -d` 后台运行 |
| **PC 关机了还能收消息吗？** | 不能。但可以买树莓派 Zero ($10) 24h 跑 Broker |
| **外网可以推送吗？** | 可以，Broker 绑 0.0.0.0，端口映射到公网即可 |
| **ESP8266 断线重连？** | `AT+MQTTCONN` 会返回 ERROR，重试即可 |
| **QoS 选哪个？** | `qos=1`，至少一次送达，刚好适合 OTA |

---

### 2.7 替代方案二：HTTP 轮询（无需 Broker）

#### 原理

每块板**定期主动请求** HTTP 服务器，检查是否有新固件。比 MQTT 简单（不需要 Broker），但实时性差（取决于轮询间隔）。

#### 架构

```
每块板单独定时请求:

板子 ──HTTP GET──► PC HTTP Server :8080/version.txt
                ◄── version: 3

板子 ──HTTP GET──► PC HTTP Server :8080/firmware.bin
                ◄── 二进制固件数据
```

#### PC 端（只需要 HTTP 服务器）

```powershell
cd build/Debug
python -m http.server 8080
# 目录下放:
# ├── firmware.bin    ← 固件文件
# └── version.txt     ← 内容: "3"
```

#### F407 固件集成

```
ESP8266 AT 命令:
AT+HTTPGET="http://192.168.0.219:8080/version.txt"
  → +IPD,<len>:3\r\n    ← 服务器返回 "3"

AT+HTTPGET="http://192.168.0.219:8080/firmware.bin"
  → +IPD,<len>:<二进制数据>\r\n
  → +IPD,... (大文件分块)
```

**MCU 端主循环新增分支：**

```c
// 每 5 分钟执行一次:
if (osKernelGetTickCount() - last_check > pdMS_TO_TICKS(300000)) {
    last_check = osKernelGetTickCount();
    
    // 请求版本号
    ESP8266_Cmd("AT+HTTPGET=\"http://192.168.0.219:8080/version.txt\"",
                "OK", NULL, 5000);
    // 解析返回的版本号文本
    // 与本地 ota_count 比较
    
    if (new_version > current_version) {
        // 开始 HTTP 下载固件
        ESP8266_Cmd("AT+HTTPGET=\"http://192.168.0.219:8080/firmware.bin\"",
                    "OK", NULL, 5000);
        // +IPD 数据 → 写 SPI Flash
    }
}
```

#### 三种方案对比

| 特性 | MQTT 订阅 | HTTP 轮询 | 顺序 TCP 推送 (现有) |
|:-----|:---------:|:---------:|:------------------:|
| **一对多广播** | ✅ 原生支持 | ✅ 各自下载 | ❌ 逐板推送 |
| **实时性** | 毫秒级 | 分钟级 (取决于间隔) | 秒级 |
| **需额外服务** | mosquitto Broker | ❌ 无 | ❌ 无 |
| **PC 端依赖** | `paho-mqtt` 包 | Python 自带 | Python 自带 |
| **板端复杂度** | 中 (MQTT命令+JSON解析) | 低 (HTTP GET) | 低 (TCP socket) |
| **断网续传** | 自动重连 | 下次轮询 | 需手动重试 |
| **ESP8266 AT 版本要求** | ≥ v2.1.0 | ≥ v2.1.0 | 无要求 |
| **推荐场景** | **多个板、频繁更新** | **少量板、懒人配置** | **单板开发调试** |

---

## 三、CAN ISO-TP 传输（传统 OTA）

### 3.1 架构

```
PC ──UART──► F103(网关) ──CAN 500kbps──► F407 CAN1 ──► Bootloader ISO-TP 接收
                                                   ──► 内部 Flash 写入
                                                   ──► 跳转新 APP
```

### 3.2 硬件链路

```
┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
│  PC (发送工具)    │     │  F103C8T6 (网关)  │     │  F407ZGT6        │
│                  │     │                  │     │                  │
│ can_ota_send.py  │     │ 接收到串口数据    │     │ CAN ISO-TP 解析  │
│  ┌───────────┐   │UART │  → 封装 CAN 帧   │ CAN │  ┌────────────┐  │
│  │.bin 固件  │───┼────►│  → 发送 ID=0x0B1 │────►│  │iso_tp_cfg  │  │
│  │           │   │     │                  │     │  │.c 重组固件  │  │
│  └───────────┘   │     │ 网关模式:        │     │  │ → 写 Flash  │  │
│                  │     │ #define GATEWAY  │     │  └────────────┘  │
└──────────────────┘     └──────────────────┘     └──────────────────┘
```

### 3.3 CAN 协议

| CAN ID | 方向 | 长度 | 格式 | 用途 |
|:------:|:----:|:----:|------|------|
| `0x12` | F103→F407 | 6B | `[T+40][Tdec][H][Hdec][knob_H:key][knob_L]` | 传感器数据 |
| `0x0B1` | F103→F407 | 8B | ISO-TP 流, 首字节 `0xBF` | **OTA 固件数据** |
| `0x0B3` | F103→F407 | 5B | `[BE][AD][BE][EF][01]` | **CALL_OTA 触发** |

ISO-TP (ISO 15765-2) 协议：
```
单帧 (SF):   [0xBF] [len] [data...]
首帧 (FF):   [0x10] [len_hi:len_lo] [data...]
连续帧 (CF): [0x20] [seq] [data...]
流控帧 (FC): [0x30] [BS] [STmin]
```

### 3.4 时序

```
APP 运行中                              F103 网关                  Bootloader
  │                                      │                          │
  │  CAN CALL_OTA (ID=0x0B3) ───────────►│                          │
  │                                      │                          │
  │  设 ota_flag → NVIC_SystemReset()                              │
  │ ────────────────────────────────────────────────────────────►   │
  │                                                                 │
  │                                         检测 ota_flag=1          │
  │                                         进入 ISO-TP 接收         │
  │                                                                 │
  │                                      CAN ISO-TP 帧 ──────────►  │
  │                                      CAN ISO-TP 帧 ──────────►  │
  │                                      ...                         │
  │                                      CAN ISO-TP 帧 ──────────►  │
  │                                                                 │
  │                                             校验 → 验签 → 跳转   │
```

### 3.5 CAN vs WiFi 对比

| 特性 | CAN ISO-TP | WiFi 定向 | WiFi 广播(未实现) |
|:-----|:----------:|:---------:|:----------------:|
| **通信介质** | CAN 总线 (差分双绞线) | 802.11 WiFi | 802.11 WiFi |
| **速率** | 500kbps → ~40KB/s | ~6.1 KB/s | ~6.1 KB/s |
| **470KB 耗时** | ~12s | ~75s | ~75s |
| **距离** | ~40m (CAN) | ~100m (WiFi) | ~100m (WiFi) |
| **可靠性** | 高 (CAN 硬件 CRC) | 中 (TCP 重传) | 低 (UDP 丢包) |
| **多板支持** | 总线所有节点同时收 | 逐板推送 | 无法实现 |
| **是否需要路由器** | ❌ 不需要 (CAN 直连) | ✅ 需要 | ✅ 需要 |
| **是否需要调试线** | ❌ 不需要 | ❌ 不需要 | ❌ 不需要 |
| **开发复杂度** | 高 (ISO-TP 协议栈) | 低 (TCP Socket) | — |
| **硬件成本** | CAN 收发器 ~$1 | ESP8266 ~$2 | ESP8266 ~$2 |

### 3.6 CAN 关键代码路径

| 文件 | 函数 | 职责 |
|------|------|------|
| `Bootloader/Src/iso_tp_cfg.c` | `iso_tp_init()`, `iso_tp_poll()` | CAN ISO-TP 协议解析 |
| `Bootloader/Src/isotp.c` | ISO-TP 单帧/多帧解析 | 固件包重组 |
| `Bootloader/Src/main.c` | `main()` OTA 分支 | 检测 flag → 接收 → 验签 → 跳转 |
| `scripts/can_ota_send.py` | `send_firmware()` | PC 端 CAN 固件发送工具 |

---

## 四、三种模式决策对比

| 场景 | 推荐模式 | 理由 |
|:-----|:--------:|:-----|
| **开发调试** (单板, SWD 在手边) | SWD 直写 | 秒级烧录, 无传输过程 |
| **单板无线更新** (已部署, 有 WiFi) | **WiFi 定向** | 已验证通过, ~75s/470KB |
| **实验室多板更新** (多板, 有 WiFi) | **WiFi 顺序推送** | 现有功能, N × ~95s |
| **现场多板更新** (车内 CAN 总线) | **CAN ISO-TP** | 无需网络, 高可靠 |
| **量产批量更新** (>10板, 有 WiFi) | HTTP 轮询 (待实现) | 每板自检, 错峰下载 |

---

## 五、传输协议栈一览

```
                        WiFi 定向                    CAN ISO-TP
               ┌──────────────────────┐    ┌──────────────────────┐
应用层         │  OTA:<size> + 二进制  │    │  ISO-TP 多帧传输     │
传输层         │  TCP (可靠)           │    │  ISO 15765-2         │
网络层         │  IP                   │    │  —                   │
链路层         │  WiFi 802.11          │    │  CAN 2.0A (11-bit)   │
物理层         │  2.4GHz               │    │  CAN_H/CAN_L 差分    │
               │  ESP8266 + USART3     │    │  TJA1050 + CAN1      │
               └──────────────────────┘    └──────────────────────┘
                                        ↓                    ↓
                               Bootloader 接收层      Bootloader 接收层
                               ┌──────────────┐    ┌──────────────────┐
                               │ SPI Flash     │    │ 直接写入内部 Flash│
                               │ → 备份 → 恢复  │    │ (iso_tp_cfg.c)   │
                               └──────────────┘    └──────────────────┘
                                        ↓                    ↓
                              验证: HMAC-SHA256 签名 (@ 0x080DFF80)
                                        ↓
                              跳转: APP @ 0x08010000
```

---

## 六、附录：常用命令速查

```bash
# WiFi 定向推送
python scripts/wifi_ota_send.py 192.168.0.235 build/Debug/MY_OTA_GUI.bin

# WiFi 定向 + UID 握手
python scripts/wifi_ota_send.py 192.168.0.235 build/Debug/MY_OTA_GUI.bin --who

# CAN ISO-TP 推送
python scripts/can_ota_send.py build/Debug/MY_OTA_GUI.bin

# Ymodem 串口推送
python scripts/ymodem_send.py build/Debug/MY_OTA_GUI.bin COM3

# SWD 直写 (开发调试用)
openocd ... -c "program build/Debug/MY_OTA_GUI.elf verify"
            -c "program build/Debug/signature.bin 0x080DFF80 verify"
```
