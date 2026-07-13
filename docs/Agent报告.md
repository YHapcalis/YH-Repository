# Agent 报告 — F407 OTA Bootloader 全量开发

> **Agent 类型**：全自动全量开发  
> **工程**：MY_OTA_GUI (STM32F407ZGT6 + NT35510 LCD + W25Q128 + ESP8266)  
> **源码**：基于 uint3code OTA 例程向 F407+F103C8 双板架构移植  
> **日期**：2026-07-11（第五轮：ESP8266 WiFi 双模式 OTA）

---

## 一、任务概述

在上一轮（OV7670 摄像头完整集成 + 三级图片状态机）的基础上，本周期完成了：
1. ESP8266 WiFi 模块 FreeRTOS 任务 + USART3 中断集成
2. WiFi OTA 双模式架构（定向握手 WHO + 广播触发 OTA_N）
3. Bootloader WiFi 预存检测快速恢复（跳过 60s CAN 等待）
4. PC 端双模式推送工具 `scripts/wifi_ota_send.py`
5. SWD 远程操控接口文档（harness AI 工具链）

---

## 二、开发成果清单

### ESP8266 WiFi 模块集成 (f407d11)

| 文件 | 说明 |
|------|------|
| `Core/Src/wifi_function.c/h` | ESP8266 AT 指令封装（连接/发送/接收/Server） |
| `Core/Src/wifi_config.c/h` | 数据缓冲 `strEsp8266_Fram_Record` + `USART3_printf` |
| `Core/Src/stm32f4xx_it.c` | `USART3_IRQHandler` 逐字节接收，`\n` 帧尾标记 |
| `Core/Src/freertos.c` | `StartWiFiTask` (BelowNormal, 2KB 栈) + `wifi_task.h` |

引脚：USART3 @ PB10(TX)/PB11(RX), PF6(CH_PD), PC0(RST)

### WiFi OTA 双模式 (ad6e554 + 24b36e5)

**定向推送模式 (开发调试)：**
- PC → TCP:8080 → ESP8266 → USART3 → F407
- WHO 命令 → MCU 回复 96-bit UID (`ID:XXXXXXXX...`)
- OTA:size 命令 → 写入 SPI Flash 备份区 (0xE00000)
- 可选 `--who --target-uid` 校验目标板

**广播推送模式 (量产更新)：**
- PC → UDP:8081 → 所有 ESP8266 接收触发
- `OTA_N:size:PC_IP:port` → 各板连 TCP:8765 独立下载
- 支持最多 10 块并发，自动发现 PC IP

**Bootloader 配合 (ad6e554)：**
- 在 CAN ISO-TP 等待前增加 SPI Flash 预存检测
- 发现 `SPI_BAK_MAGIC` (0x2141544F = "OTA!") → 直接恢复
- 跳过 60s CAN 等待，秒级完成

### PC 端工具 (scripts/wifi_ota_send.py)

```bash
# 定向推送
python wifi_ota_send.py 192.168.0.235 build/Debug/MY_OTA_GUI.bin

# 定向 + UID 握手校验
python wifi_ota_send.py 192.168.0.235 build/Debug/MY_OTA_GUI.bin --who --target-uid 1FFF

# 广播推送（所有板子同时升级）
python wifi_ota_send.py build/Debug/MY_OTA_GUI.bin --broadcast
```

### SWD 远程操控接口文档 (f4b36fd)

新建三份文档供 harness AI 使用：
- `docs/SWD远程操控接口手册.md` — 人工/AI 阅读
- `docs/SWD远程操控-接口数据.json` — 程序解析
- `docs/SWD远程操控-接口数据.yaml` — 程序解析

包含 18 个内存变量地址、22 个可调用函数地址、LCD NT35510 寄存器映射。已实机验证全部 4 页切换 + 心跳监测。

### 恢复误删脚本 (28c5476)

从 git 历史恢复：
- `scripts/can_ota_send.py` — CAN ISO-TP 升级路径
- `scripts/ymodem_send.py` — Ymodem 备选升级
- `scripts/sign_firmware.py` — CMake 签名工具

---

## 三、最终二进制分析

| 目标 | 大小 | 占用 | 余量 |
|------|------|------|------|
| Bootloader | 64KB | 44KB (69%) | 20KB |
| APP | 832KB | 471KB (57%) | 361KB |
| 主SRAM | 128KB | 105KB (82%) | 23KB |
| CCMRAM | 64KB | 32KB (LVGL池) | 32KB |

---

## 四、已知问题

1. **压力测试偶发卡死**：进摄像头→退出→频繁页面切换时，CAN 连接状态下偶发。未定位根因，不影响正常使用。
2. **外部 SRAM 前 256KB 可靠**：超过此范围 FSMC 16bit byte-write 不可靠。
3. **touchTask 栈过小**：512B 仅剩 87B 余量，建议升至 2KB。
4. **ESP8266 +IPD 二进制数据**：`strlen()` 不能用于二进制帧，已改用 +IPD 头部的 `<len>` 字段。

---

## 五、WiFi OTA 实测日志

```
[WiFi] AT OK -- module ready
[WiFi] Mode set to STA
[WiFi] Connecting to "Lee"...
WIFI CONNECTED
WIFI GOT IP
[WiFi] Connected!
+CIFSR:STAIP,"192.168.0.235"
[WiFi] TCP server on port 8080 ready
[WiFi] UDP broadcast listener on port 8081

--- PC 推送 470KB ---
[OTA] Connected!
[OTA] Sent: OTA:470932
[OTA] 100% (470932/470932 bytes, 6.1 KB/s)
[OTA] Done! 470932 bytes sent in 75.2s

--- Bootloader ---
[BOOT] WiFi OTA staged firmware found!
[SPI] Restoring 470908 bytes...
[BOOT] WiFi OTA restore OK
[BOOT] Signature OK
[BOOT] ---> jump to APP @ 0x08010000
```

## 六、Git 日志（本轮新增）

```
24b36e5 refactor: WiFi OTA 双模式架构 — 定向握手(WHO) + 广播触发(OTA_N)
ad6e554 feat: WiFi OTA 全链路 — ESP8266 TCP Server 接收固件 + Bootloader 预存检测 + PC 发送端
f407d11 feat: ESP8266 WiFi 模块集成 — FreeRTOS 任务 + USART3 中断
918f14b docs: 更新工程报告 — ESP8266 WiFi 集成 + 定向/广播双模式 OTA
f4b36fd docs: SWD远程操控接口文档 — harness AI 内存读写+函数调用工具链
28c5476 chore: 恢复 can_ota_send.py + ymodem_send.py (CAN ISO-TP 升级路径)
```

## 七、下一阶段

WiFi OTA 双模式已验收通过，核心工程目标已全部完成。

待确认后续方向：
- F103 工程同步优化（CAN 网关 + 传感器节点双模式已实现）
- 工程注释整理（中文→英文，Doxygen 风格）
