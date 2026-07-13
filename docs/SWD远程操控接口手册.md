# SWD 远程操控接口手册

> **给 harness AI 用** — 通过 OpenOCD TCL + SWD 直接读写 STM32F407 内存/调用固件函数
>
> 目标固件: **MY_OTA_GUI** (APP @ 0x08010000) | 构建版本: `fbe9983` | 最后更新: 2026-07-11

---

## ⚠ 实测关键发现（必须遵守）

| # | 规则 | 说明 |
|---|------|------|
| 1 | **用 `resume [map_addr]`，不用 `reg pc + resume`** | `reg pc` 改变会被 `resume` 忽略，导致 HardFault。`resume` 会自动设置 PC 并处理 Thumb 位。地址直接用 `.map` 文件中的链接地址（不需要 +1）。 |
| 2 | **子页面之间不可直接切换** | Mode ↔ Settings ↔ 时钟 ↔ 摄像头 之间不能直接跳转。必须先返回 Home（调用 `xxx_ui_hide`），等待 5 秒渲染完成，再切到目标页。否则大概率卡死。 |
| 3 | **页面切换后等 5 秒** | 每切一次页面必须 `sleep 5000`，给 LVGL 动画+渲染留足时间。 |
| 4 | **心跳监测看 `uwTick`** | `0x200002ac` 是 SysTick 1ms 计数器。正常运行在 5 秒内增 ~5000。如果增量为 0 或负值→系统卡死。 |

### 正确 vs 错误示例

```tcl
# ✅ 正确：
halt
resume 0x08043088    # mode_ui_show 的 .map 地址（不加1）
sleep 5000
halt
set tick [read_memory 0x200002ac 32 1]  # 检查心跳

# ❌ 错误（会导致 HardFault）：
halt
reg pc 0x08043089    # 错误！resume 会忽略 reg pc
resume
```

---

## 一、内存变量读写

通过 `read_memory` / `mww` / `mwb` / `mwh` 直接操作 SRAM。

### 快速操作表

| 变量 | 地址 | 大小 | R/W | 用途 |
|------|:----:|:----:|:---:|------|
| `g_ota_pending` | **0x20017c17** | 1B | RW | ★ 写1触发OTA |
| `g_theme_id` | **0x20017c14** | 1B | RW | 主题: 0暗黑/1明亮/2护眼 |
| `g_camera_active` | **0x20017d90** | 1B | RW | 摄像头激活标志 |
| `g_pressed` | **0x20017b80** | 1B | RW | 触摸按下 |
| `g_last_x` | **0x20017b7e** | 2B | RW | X坐标(0~799) |
| `g_last_y` | **0x20017b7c** | 2B | RW | Y坐标(0~479) |
| `g_touch` | **0x20017b3c** | 22B | RW | 完整触摸数据 |
| `s_fallback_active` | **0x200000c4** | 1B | R | 0=后备已释放 |
| `g_width` | **0x200000c2** | 2B | R | LCD宽 |
| `g_height` | **0x200000c0** | 2B | R | LCD高 |
| `ui_scr_main` | **0x20017c50** | 4B | R | 主页screen指针 |
| `ui_scr_mode` | **0x20017cb8** | 4B | R | Mode页screen指针 |
| `ui_scr_settings` | **0x20017ccc** | 4B | R | 设置页screen指针 |
| `ui_scr_clock` | **0x20017d8c** | 4B | R | 时钟页screen指针 |
| `ui_scr_camera` | **0x20017d94** | 4B | R | 摄像头页screen指针 |

### `g_touch` 结构体布局 (22B)

```
0x20017b3c: x[0] x[1] x[2] x[3] x[4]  (uint16_t × 5, 10B)
0x20017b46: y[0] y[1] y[2] y[3] y[4]  (uint16_t × 5, 10B)
0x20017b50: sta                          (uint8_t, 1B)
0x20017b51: points                       (uint8_t, 1B)
```

### TCL 示例

```tcl
# 读变量
read_memory 0x20017c14 32 1    # 读 g_theme_id
read_memory 0x20017b7e 16 1    # 读 g_last_x (16-bit)
read_memory 0x20017b3c 16 11   # 读 g_touch 全部 (22B = 11 half-words)

# 写变量
mwb 0x20017c17 1               # 触发 OTA
mwb 0x20017c14 1               # 切换主题为"明亮"
mwh 0x20017b7e 400             # 模拟触摸 X=400
mwh 0x20017b7c 200             # 模拟触摸 Y=200
mwb 0x20017b80 1               # 模拟按下
```

---

## 二、函数调用

### 调用方法

```tcl
# 通用步骤:
halt                                    # 1. 暂停 CPU
resume 0x080XXXXX                       # 2. 执行函数（地址用 .map 值，不加1）
sleep 5000                              # 3. 等 LVGL 渲染
halt                                    # 4. 停住检查
read_memory 0x200002ac 32 1             # 5. 读 uwTick 检查心跳
resume                                  # 6. 恢复系统
```

> 有参数的函数，需先在 halt 状态下设寄存器：
> ```tcl
> halt
> reg r0 0x41CC0000    # 参数1 (float 25.5)
> reg r1 0x42700000    # 参数2 (float 60.0)
> resume 0x08042370    # 调用 app_ui_update_sensor
> ```

### 页面切换函数（实测全部通过 ✓）

| 操作 | `.map` 地址 | 说明 |
|------|:----------:|------|
| Home → **Mode** | `resume 0x08043088` | 切换到 info 页 |
| Mode → **Home** | `resume 0x080430CC` | 返回主页 |
| Home → **设置** | `resume 0x080434C0` | 切换到设置页 |
| 设置 → **Home** | `resume 0x080434E8` | 返回主页 |
| Home → **时钟** | `resume 0x08044648` | 切换到时钟页 |
| 时钟 → **Home** | `resume 0x080445B4` | 返回主页 |
| Home → **摄像头** | `resume 0x080448E8` | 切换到摄像头页 |
| 摄像头 → **Home** | `resume 0x0804499C` | 返回主页 |

### 数据注入函数

| 函数 | 地址 | 参数 | 说明 |
|------|:----:|------|------|
| `app_ui_update_sensor` | **0x08042370** | R0=temp(float), R1=hum(float), R2=knob(uint16), R3=key_id(uint8) | 注入传感器数据 |
| `app_ui_update_time` | **0x08042518** | R0=year..R3=hour, 栈传min/sec | 更新时间 |

### 其他系统函数

| 函数 | 地址 | 说明 |
|------|:----:|------|
| `touch_poll` | **0x0804190C** | 执行一次触摸扫描 |
| `backup_is_valid` | **0x080438F0** | 返回 R0=0/1 备份区是否有效 |
| `backup_erase` | **0x08043AE4** | 擦除内部 Flash 扇区9 |
| `spi_img_load_all` | **0x08043BF4** | 重新加载图片到 SRAM |

### LCD 硬件函数

| 函数 | 地址 | 参数 | 说明 |
|------|:----:|------|------|
| `NT35510_WritePixel` | **0x08040D3A** | R0=RGB565 | 连续写像素（需先调 SetWindow） |
| `NT35510_SetWindow` | **0x08040CB8** | R0=x, R1=y, R2=w, R3=h | 设置写窗口 |

> 注意: `NT35510_DrawPixel`(x,y,color) 和 `NT35510_Clear` 被编译器内联，无可调用地址。写像素用 `SetWindow`+`WritePixel` 组合。

### SPI Flash 函数

| 函数 | 地址 | 说明 |
|------|:----:|------|
| `EN25Q128_ReadID` | **0x080407E0** | 返回 JEDEC ID (W25Q128=0xEF4018) |
| `EN25Q128_BackupFirmware` | **0x080409E4** | 裸备份固件 |
| `EN25Q128_BackupFirmwareLFS` | **0x08040B20** | LFS 备份固件 |

### TCL 完整示例

```tcl
# ★ 页面切换（已验证可行）
halt
resume 0x08043088        # → Mode 页
sleep 5000
halt
read_memory 0x200002ac 32 1   # 检查心跳
resume

# ★ 注入传感器数据（模拟 F103 CAN 节点）
halt
reg r0 0x41CC0000        # temp = 25.5
reg r1 0x42700000        # hum = 60.0
reg r2 512               # knob
reg r3 1                 # key_id
resume 0x08042370
sleep 2000
halt
read_memory 0x200002ac 32 1
resume

# ★ 触发 OTA
mwb 0x20017c17 1

# ★ 切换主题
halt
resume 0x080434C0        # → 设置页
sleep 5000
halt
mwb 0x20017c14 1         # 直接写变量切换主题
resume
sleep 500
halt
resume 0x080434E8        # 返回主页
sleep 5000
```

---

## 三、LCD 寄存器（NT35510）

LCD 通过 FSMC 16-bit 8080 接口映射，基址 `0x6C00007E`。

```
CMD 寄存器: *(volatile uint16_t*)0x6C00007E  (RS=0)
DAT 寄存器: *(volatile uint16_t*)0x6C000080  (RS=1)
```

### 常用命令

| 命令 | 码值 | 说明 |
|------|:----:|------|
| SW_RESET | `0x0001` | 软件复位 |
| SLEEP_OUT | `0x0011` | 退出睡眠（开显示） |
| DISPLAY_ON | `0x0029` | 开显示 |
| DISPLAY_OFF | `0x0028` | 关显示 |
| COLUMN_ADDR | `0x002A` | 列地址设置（xS, xE 各2B） |
| PAGE_ADDR | `0x002B` | 行地址设置（yS, yE 各2B） |
| MEMORY_WRITE | `0x002C` | 连续写 RGB565 像素 |

```tcl
# 清屏为蓝色
mwh 0x6C00007E 0x002A; mwh 0x6C000080 0; mwh 0x6C000080 799
mwh 0x6C00007E 0x002B; mwh 0x6C000080 0; mwh 0x6C000080 479
mwh 0x6C00007E 0x002C
# 接下来循环写入 800×480 个 0x001F（蓝色像素）
```

> 大批量写像素时效率低，推荐用 `SetWindow`+`WritePixel` 固件函数代替逐像素写。

---

## 四、内存分区

| 区域 | 地址 | 大小 | 说明 |
|------|:----:|:----:|------|
| Bootloader | `0x08000000` | 64KB | 二级引导 |
| APP 固件 | `0x08010000` | 832KB | 主固件 |
| 备份扇区9 | `0x080A0000` | 128KB | 图片降级备份 (MAGIC=0x494D474D) |
| 签名块 | `0x080DFF80` | 40B | fw_size+hmac+magic |
| 参数扇区 | `0x080E0000` | 128KB | OTA 配置 |
| 主 SRAM | `0x20000000` | 128KB | 运行时内存 |
| CCMRAM | `0x10000000` | 64KB | LVGL 池 32KB |
| 外部 SRAM | `0x68000000` | 1MB | FSMC, 图片缓存 |
| LCD 控制器 | `0x6C00007E` | — | NT35510 FSMC |

---

## 五、调用约定

| 项目 | 说明 |
|------|------|
| 架构 | ARM Cortex-M4F (ARMv7E-M) |
| 指令集 | Thumb-2（仅 Thumb 模式） |
| 调用约定 | AAPCS |
| 参数传递 | R0~R3 前 4 个参数，多余压栈 |
| 返回值 | R0 |
| float 编码 | IEEE 754 单精度（如 25.5 → 0x41CC0000） |
| 安全返回地址 | LR 设 0x080FFFFE（备用，实际被 `resume [addr]` 覆盖） |

### Float 编码速查

| 值 | IEEE 754 十六进制 |
|:---:|:-----------------:|
| 0.0 | 0x00000000 |
| 25.5 | 0x41CC0000 |
| 60.0 | 0x42700000 |
| 100.0 | 0x42C80000 |

---

## 六、AI 工具定义（供注册 Tool Registry）

### Tool 1: `stm32_mem_rw`

读写 STM32 SRAM 变量。

```json
{
  "name": "stm32_mem_rw",
  "parameters": {
    "action": {"enum": ["read", "write"]},
    "var": {"enum": [
      "g_ota_pending", "g_theme_id", "g_camera_active",
      "g_pressed", "g_last_x", "g_last_y",
      "g_touch", "g_can_sensor",
      "s_fallback_active",
      "ui_scr_main", "ui_scr_mode",
      "ui_scr_settings", "ui_scr_clock", "ui_scr_camera"
    ]},
    "value": {"description": "write 时填"}
  }
}
```

### Tool 2: `stm32_func_call`

让 CPU 执行固件函数。

```json
{
  "name": "stm32_func_call",
  "parameters": {
    "func": {"enum": [
      "mode_ui_show", "mode_ui_hide",
      "settings_ui_show", "settings_ui_hide",
      "clock_ui_show", "clock_ui_hide",
      "camera_ui_show", "camera_ui_hide",
      "touch_poll", "backup_is_valid",
      "backup_erase", "spi_img_load_all"
    ]},
    "wait_ms": {"default": 5000}
  }
}
```

### Tool 3: `stm32_inject_sensor`

模拟 CAN 传感器数据注入。

```json
{
  "name": "stm32_inject_sensor",
  "parameters": {
    "temp": {"type": "number", "description": "温度 °C"},
    "hum": {"type": "number", "description": "湿度 %"},
    "knob": {"type": "integer", "description": "旋钮 0~4095"},
    "key_id": {"type": "integer"},
    "key_type": {"type": "integer"}
  }
}
```

---

## 附录：完整函数地址表

> 所有地址来自 `build/Debug/MY_OTA_GUI.map`。
> 调用时用 `resume [link_address]`，**不需要 +1**。

| 符号 | link_address | 说明 |
|------|:-----------:|------|
| `mode_ui_show` | 0x08043088 | → Mode |
| `mode_ui_hide` | 0x080430CC | Mode → 主页 |
| `settings_ui_show` | 0x080434C0 | → 设置 |
| `settings_ui_hide` | 0x080434E8 | 设置 → 主页 |
| `clock_ui_show` | 0x08044648 | → 时钟 |
| `clock_ui_hide` | 0x080445B4 | 时钟 → 主页 |
| `camera_ui_show` | 0x080448E8 | → 摄像头 |
| `camera_ui_hide` | 0x0804499C | 摄像头 → 主页 |
| `app_ui_update_sensor` | 0x08042370 | 注入传感器 |
| `app_ui_update_time` | 0x08042518 | 更新时间 |
| `touch_poll` | 0x0804190C | 触摸扫描 |
| `touch_init` | 0x08041800 | 触摸初始化 |
| `backup_is_valid` | 0x080438F0 | 备份区有效? |
| `backup_erase` | 0x08043AE4 | 擦备份区 |
| `backup_write_all_from_lfs` | 0x08043AF8 | LFS→备份区 |
| `spi_img_load_all` | 0x08043BF4 | 加载图片 |
| `spi_load_to_ram` | 0x08043908 | LFS→SRAM |
| `NT35510_WritePixel` | 0x08040D3A | 写像素（需SetWindow） |
| `NT35510_SetWindow` | 0x08040CB8 | 设置写窗口 |
| `NT35510_Init` | 0x08040C58 | LCD 初始化 |
| `NT35510_SetDir` | 0x08040BFC | 设置方向 |
| `EN25Q128_Init` | 0x08040994 | SPI Flash 初始化 |
| `EN25Q128_Read` | 0x080408E4 | SPI Flash 读 |
| `EN25Q128_Write` | 0x080408B8 | SPI Flash 写 |
| `EN25Q128_EraseSector` | 0x0804087C | 扇区擦除 |
| `EN25Q128_ReadID` | 0x080407E0 | 读 JEDEC ID |
| `EN25Q128_BackupFirmware` | 0x080409E4 | 裸备份 |
| `EN25Q128_BackupFirmwareLFS` | 0x08040B20 | LFS 备份 |
| `app_ui_create` | 0x0804230C | 重建 UI |
| `ui_get_theme` | 0x0804271C | 读主题 |
| `ui_apply_theme` | 0x08042728 | 应用主题 |
| `lv_scr_load_anim` | 0x08014E70 | LVGL 底层切屏 |

> **重新编译后地址可能变化**，更新方法:
> ```bash
> grep -E "\s+0x080[0-9a-f]{5}\s+(mode_ui_show|settings_ui_show|clock_ui_show|camera_ui_show|app_ui_update_sensor|EN25Q128_ReadID|spi_img_load_all|backup_is_valid|touch_poll)\b" build/Debug/MY_OTA_GUI.map
> ```
