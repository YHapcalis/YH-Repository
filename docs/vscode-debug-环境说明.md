# F407 OTA 双区工程 — VSCode 调试环境说明

---

## 环境一览

| 项目 | 内容 |
|------|------|
| 主控 | STM32F407ZGT6 |
| 调试器 | ST-LINK V2 (20pin SWD) |
| OpenOCD | STM32CubeIDE 内置版 |
| 扩展 | Cortex-Debug |
| 架构 | Bootloader (64KB) + APP (832KB) 双区 OTA |

---

## 相关文件

```
.vscode/
  ├── tasks.json      构建 + 烧录任务
  └── launch.json     调试配置（APP / BL 两个入口）

openocd.cfg           调试器参数
```

---

## 任务快捷键 (Ctrl+Shift+B)

### 构建

| 任务 | 说明 | 快捷键 |
|------|------|--------|
| CMake: 配置 (Debug) | 首次或改 CMakeLists.txt 后用 | — |
| **CMake: 构建 (Debug)** | 并行编译 APP + BL + 自动签名 | **默认** |
| CMake: 清理+重新构建 | 完整重建 | — |

> 构建产物在 `build/Debug/`，自动签名生成 `signature.bin` (40B @ 0x080DFF80)

### 烧录

| 任务 | 烧录内容 | 用途 |
|------|----------|------|
| **Flash: 烧录 (OpenOCD)** | BL + APP + 签名 + 参数，共 4 段 | **日常烧录首选** |
| Flash: 仅烧录 APP | APP + 签名 | 只改 APP 代码 |
| 初始化参数扇区 | param_init.bin @ 0x080E0000 | 新芯片 / 参数损坏 |
| 触发 OTA 模式 | param_ota.bin @ 0x080E0000 | 手动进 OTA |

### 组合任务

| 任务 | 行为 |
|------|------|
| **构建+烧录** | 先构建 → 再完整烧录 |

---

## 调试入口 (F5)

两个入口，在 VSCode 调试面板中选择：

| 名称 | 加载 ELF | 入口地址 |
|------|----------|----------|
| **ST-LINK V2 (OpenOCD)** | `MY_OTA_GUI.elf` | 0x08010000 (APP) |
| **Bootloader (OpenOCD)** | `bootloader.elf` | 0x08000000 (BL) |

启动后自动 halt 在 `main()`，连接方式 `monitor reset halt`。

---

## 双区架构差异

相比单区工程：

**1. 两个独立 ELF**，各有链接脚本
- APP: `STM32F407XX_FLASH_APP.ld` → 0x08010000
- BL: `STM32F407XX_FLASH_BL.ld` → 0x08000000

**2. 烧录 4 段不是 1 段**

| 段 | 地址 | 大小 |
|---|------|------|
| bootloader.elf | 0x08000000 | ~44KB |
| MY_OTA_GUI.elf | 0x08010000 | ~421KB |
| signature.bin | 0x080DFF80 | 40B |
| param_init.bin | 0x080E0000 | 12B |

**3. 参数扇区**: 0x080E0000 存 OTA 标志 + 校验。新芯片必须先烧 param_init.bin。

---

## 踩坑记录

### PowerShell 分号截断

**症状**: 烧录任务报错，OpenOCD 不执行
**原因**: VSCode 默认用 PowerShell 跑 shell 任务，`-c` 参数里的 `;` 被当成命令分隔符截断
**修复**: 所有 OpenOCD 任务强制用 cmd.exe：

```json
"options": {
    "shell": {
        "executable": "cmd.exe",
        "args": ["/d", "/c"]
    }
}
```

### OpenOCD 命令语法

```
✗  verify; reset exit
✓  verify; reset; exit
```

### cortex-debug 配置

用 `"runToEntryPoint": "main"`，不要用 `"runToMain": true`（已废弃）。

---

## 快捷键速查

| 操作 | 按键 |
|------|------|
| 构建（默认） | `Ctrl+Shift+B` |
| 构建 + 完整烧录 | `Ctrl+Shift+B` → 选 Flash: 烧录 (OpenOCD) |
| 开始调试 | `F5` |
| 重启调试 | `Ctrl+Shift+F5` |

**首次烧录**（全新芯片 / 参数损坏）：
1. `Ctrl+Shift+B` → 选"初始化参数扇区"
2. `Ctrl+Shift+B` → 选"Flash: 烧录 (OpenOCD)"

---

## 终端命令行 (Git Bash)

以下脚本适用于 CI 或 AI 自动化烧录。

### 路径变量

```bash
OPENOCD="E:/ST/STM32/STM32CubeIDE/plugins/.../openocd.exe"
SCRIPTS="E:/ST/STM32/STM32CubeIDE/plugins/.../st_scripts"
WORKSPACE="E:/ST/STM32/MY_workspace/Projects_F407/MY_OTA_GUI"
BUILD="${WORKSPACE}/build/Debug"
```

完整路径见 `.vscode/tasks.json`。

### 完整烧录 4 段

```bash
"$OPENOCD" -s "$SCRIPTS" -f "${WORKSPACE}/openocd.cfg" \
  -c "program ${BUILD}/bootloader.elf verify" \
  -c "program ${BUILD}/MY_OTA_GUI.elf verify" \
  -c "program ${BUILD}/signature.bin 0x080DFF80 verify" \
  -c "program ${WORKSPACE}/param_init.bin 0x080E0000 verify" \
  -c "reset; exit"
```

### 仅烧 APP

```bash
"$OPENOCD" -s "$SCRIPTS" -f "${WORKSPACE}/openocd.cfg" \
  -c "program ${BUILD}/MY_OTA_GUI.elf verify" \
  -c "program ${BUILD}/signature.bin 0x080DFF80 verify" \
  -c "reset; exit"
```

### 初始化参数扇区

```bash
"$OPENOCD" -s "$SCRIPTS" -f "${WORKSPACE}/openocd.cfg" \
  -c "program ${WORKSPACE}/param_init.bin 0x080E0000 verify" \
  -c "reset; exit"
```

### 首次烧录（全新芯片）

```bash
# Step 1: 初始化参数
"$OPENOCD" -s "$SCRIPTS" -f "${WORKSPACE}/openocd.cfg" \
  -c "program ${WORKSPACE}/param_init.bin 0x080E0000 verify" \
  -c "reset; exit"

# Step 2: 构建 + 完整烧录
cd "$WORKSPACE" && cmake --build build/Debug --parallel 8 && \
"$OPENOCD" -s "$SCRIPTS" -f "${WORKSPACE}/openocd.cfg" \
  -c "program ${BUILD}/bootloader.elf verify" \
  -c "program ${BUILD}/MY_OTA_GUI.elf verify" \
  -c "program ${BUILD}/signature.bin 0x080DFF80 verify" \
  -c "reset; exit"
```
