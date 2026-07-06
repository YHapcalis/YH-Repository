# Agent 报告 — F407 OTA Bootloader 全量开发

> **Agent 类型**：全自动全量开发  
> **工程**：MY_OTA_GUI (STM32F407ZGT6 + NT35510 LCD + W25Q128)  
> **源码**：基于 uint3code OTA 例程向 F407+F103C8 双板架构移植  
> **日期**：2026-07-06（第三轮完整总结）

---

## 一、任务概述

在上一轮（中文显示 + 三屏扩展 + SPI Flash 图片加载）的基础上，本周期完成了：
1. LittleFS 文件系统全线部署
2. 双区 OTA 回滚（LFS 文件备份 + 裸备份降级）
3. HMAC-SHA256 固件签名验证
4. 工程架构重构（Bootloader 64KB、APP 移至 0x08010000）

---

## 二、开发成果清单

### LittleFS 文件系统 (baf971e)

| 文件 | 说明 |
|------|------|
| `Core/ThirdParty/littlefs/` | lfs.h + lfs.c + lfs_util.h (官方 master) |
| `Core/Inc/lfs_port.h` | LFS 移植层头文件 |
| `Core/Src/lfs_port.c` | 4 HAL + CRC-32 + mount/format |
| `Bootloader/Src/lfs_boot.c` | BL 只读 LFS 最小移植 |
| `scripts/mklfs_host.c` + `make_lfs_image.py` | PC 端 LFS 镜像生成 |
| `scripts/patch_font.py` | 字库现修脚本（应急用，已删除） |

SPI Flash 分区：LittleFS 14MB (0x000000-0xDFFFFF) + 裸备份 512KB (0xE00000-0xE7FFFF)

### 双区 OTA 回滚 + Bootloader 64KB (23a7362)

- Bootloader 从 32KB 翻倍到 64KB（0x08000000-0x0800FFFF）
- APP 移至 0x08010000，分区缩小到 832KB
- 链接脚本 + VTOR + 全部地址常量同步更新
- 后备图片数据移除（释放 ~55KB 内部 Flash）
- 备份改为 LFS 文件 `firmware.bak`（APP 写，BL 读）
- 保留裸备份区作为降级方法
- Mode 屏备份状态英文显示

### HMAC-SHA256 固件签名 (22631f8)

| 文件 | 说明 |
|------|------|
| `Core/Inc/sha256.h` | SHA-256 + HMAC-SHA256 API |
| `Core/Src/sha256.c` | 实现 + 32 字节密钥 + verify_firmware_sig() |
| `scripts/sign_firmware.py` | PC 端签名工具（CMake POST_BUILD 自动执行） |

签名块 @ 0x080DFF80（40 字节：fw_size + HMAC + Magic）
- 签名有效 → `[BOOT] Signature OK` → 正常跳转
- 无签名 → `[BOOT] No signature, continuing (dev mode)` → 仅警告
- 签名无效 → 尝试 SPI Flash 备份恢复，失败则 LED 死循环

### 密钥管理

密钥硬编码在 sha256.c 和 sign_firmware.py 中。如需更换，两个文件同步更新后重新编译。

---

## 三、最终二进制分析

| 目标 | 大小 | 占用 | 余量 |
|------|------|------|------|
| Bootloader | 64KB | 43.6KB (68%) | 20.4KB |
| APP | 832KB | 419KB (50%) | 413KB |
| 主SRAM | 128KB | 62KB (48%) | 66KB |
| CCMRAM | 64KB | 32KB (LVGL池) | 32KB |

---

## 四、已知问题

1. **字库回退问题**：git checkout 导致未提交的字库字符丢失，当前 HEAD 字库 122 CJK，部分中文标签可能无法显示。
2. **DCMI_XCLK(PA8) 冲突**：与 IRED 共用 PA8，Camera 启用后 IRED 不可用。
3. **DCMI_PWDN(PG9) 冲突**：与 DS18B20 共用 PG9，Camera 启用后单总线不可用。
4. **外部 SRAM 前 256KB 可靠**：超过此范围 FSMC 16bit byte-write 不可靠。

---

## 五、Git 日志

```
f8eaec8 chore: 构建任务同时编译 BL + APP, 烧录含签名
f089fbd fix: 无签名时不拦截启动 — 开发模式跳过签名验证
22631f8 feat: HMAC-SHA256 固件签名
23a7362 feat: 双区 OTA 回滚 + Bootloader 64KB + LFS 文件备份
0543fad chore: 同步未提交改动
baf971e feat: LittleFS 文件系统部署
c8771e6 feat: 日历时钟页 + Home 屏布局重排 + 双栏 Mode 屏 + 时间同步
```

---

## 六、下一阶段：外设扩展（WiFi + Camera）

当前已确定扩展方向：
- **ESP8266 WiFi**：USART3 @ PB10/PB11，AT 指令通信
- **Camera (DCMI)**：DCMI 并行接口 + I2C 配置，帧存到外部 SRAM

待用户确认功能范围和整合方案后实施。
