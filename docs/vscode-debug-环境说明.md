# F407 OTA 双区工程 VSCode 调试环境说明

## 环境概览

| 项目 | 内容 |
|---|---|
| 芯片 | STM32F407ZGT6 |
| 调试器 | ST-LINK V2 (PID:0483:3748), 20pin, SWD |
| OpenOCD | STM32CubeIDE 内置版 |
| VSCode 扩展 | Cortex-Debug |
| 架构 | 双区 OTA: Bootloader(64KB) + APP(832KB) |

---

## 文件清单

```
.vscode/
  ├── tasks.json     — 构建 + 烧录任务 (6 个)
  └── launch.json    — 调试配置 (2 个: APP + Bootloader)
openocd.cfg          — OpenOCD 调试器配置
```

---

## tasks.json — 任务定义

### CMake 构建任务

| 任务 | 快捷键 | 说明 |
|---|---|---|
| `CMake: 配置 (Debug)` | — | 首次或改 CMakeLists.txt 后执行 |
| `CMake: 构建 (Debug)` | `Ctrl+Shift+B` 默认 | 并行构建 APP + Bootloader + 自动签名 |
| `CMake: 清理+重新构建` | — | `--clean-first` 完整重建 |

构建路径:
```
${workspaceFolder}/build/Debug
```

自动签名 (POST_BUILD):
- 生成 `signed.bin`
- 生成 `signature.bin` (40 bytes @ 0x080DFF80, HMAC-SHA256)

### 烧录任务

| 任务 | 烧录内容 | 用途 |
|---|---|---|
| `Flash: 烧录 (OpenOCD)` | bootloader.elf + APP.elf + signature.bin @ 0x080DFF80 + reset | **完整烧录 (首选)** |
| `Flash: 仅烧录 APP` | APP.elf + signature.bin + reset | APP 代码更新 |
| `初始化参数扇区` | param_init.bin @ 0x080E0000 + reset | 首次烧录 / 恢复出厂 |
| `触发 OTA 模式` | param_ota.bin @ 0x080E0000 + reset | 手动触发 OTA |

### 组合任务

| 任务 | 说明 |
|---|---|
| `构建+烧录` | 先构建 → 再完整烧录 (顺序执行) |

---

## launch.json — 调试配置

两个独立入口：

| 名称 | ELF | 用途 |
|---|---|---|
| `ST-LINK V2 (OpenOCD)` | `MY_OTA_GUI.elf` | 调试 APP (0x08010000) |
| `Bootloader (OpenOCD)` | `bootloader.elf` | 调试 Bootloader (0x08000000) |

共有配置：
- 启动后自动运行到 `main()` 入口
- 连接方式: `monitor reset halt` (复位后暂停)
- OpenOCD 端口: GDB=3333, TCL=6666

---

## openocd.cfg — 调试器配置

```
source [find interface/stlink-dap.cfg]
transport select dapdirect_swd

set CONNECT_UNDER_RESET 1
reset_config srst_only srst_nogate connect_assert_srst

set CLOCK_FREQ 4000            # ST-LINK V2 上限
tcl_port 6666                   # 供 stm32_monitor.py 工具

source [find target/stm32f4x.cfg]

# 退出时 halt 防止卡死
$_TARGETNAME configure -event gdb-detach { halt }
```

---

## 双区架构的关键差异

与单区工程相比，双区 OTA 工程在调试环境上的差异：

### 1. 调试入口 ×2

APP 和 Bootloader 是**两个独立 ELF**，各有不同的链接脚本和起始地址：
- APP: `STM32F407XX_FLASH_APP.ld` → 0x08010000
- Bootloader: `STM32F407XX_FLASH_BL.ld` → 0x08000000

对应两个 launch config，调试哪个就选哪个。

### 2. 烧录段数 ×4

单区只烧 1 个 ELF，本工程需要烧 **4 段**：

| 段 | 地址 | 大小 |
|---|---|---|
| bootloader.elf | 0x08000000 | ~44KB |
| MY_OTA_GUI.elf | 0x08010000 | ~421KB |
| signature.bin | 0x080DFF80 | 40B |
| param_init.bin | 0x080E0000 | 12B |

### 3. 参数扇区

`0x080E0000` (Sector 11, 128KB) 存储 OTA 标志和校验。首次烧录或参数损坏时需要烧 `param_init.bin` 初始化。

---

## 已知陷阱

### PowerShell 分号截断 (已修复)

**症状**: 烧录任务报错，OpenOCD 命令执行不完整

**原因**: VSCode 默认用 PowerShell 执行 `"type": "shell"` 任务，PowerShell 把 `-c` 参数中的 `;` 解释为命令分隔符，把 OpenOCD 命令拦腰截断。

**修复**: 所有 OpenOCD 任务强制使用 cmd.exe:

```json
"options": {
    "shell": {
        "executable": "cmd.exe",
        "args": ["/d", "/c"]
    }
}
```

### reset exit 语法

OpenOCD 中 `reset exit` 不是合法命令，需用分号分隔:

```
# 错误
-c "program ... verify; reset exit"

# 正确
-c "program ... verify; reset; exit"
```

### runToEntryPoint

当前 cortex-debug 使用 `"runToEntryPoint": "main"`，不要改用 `"runToMain": true`（已废弃）。

---

## 快速操作

```
Ctrl+Shift+B  → 构建 (默认)
Ctrl+Shift+B  → 选 "Flash: 烧录 (OpenOCD)"  → 构建+烧录
F5            → 启动调试 (当前选中的 launch config)
Ctrl+Shift+F5 → 重启调试
```

首次烧录芯片（全新芯片或参数扇区损坏）:

```
Ctrl+Shift+B → "初始化参数扇区 (ota_flag=0)"
Ctrl+Shift+B → "Flash: 烧录 (OpenOCD)"
```
