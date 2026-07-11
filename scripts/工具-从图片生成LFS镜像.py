#!/usr/bin/env python3
"""
make_lfs_image.py — 创建 LittleFS 文件系统镜像并打包为 batch_00.h

工作流程:
  1. 从 LVGL 图片源文件提取 6 张图的 raw 数据
  2. 编译 mklfs_host.exe (使用主机 GCC + LittleFS C 源码)
  3. 运行 mklfs_host.exe 创建 LittleFS 镜像 (lfs_img.bin)
  4. 将 lfs_img.bin 打包为 batch_00.h (ST-LINK 批烧格式)

用法:
  python make_lfs_image.py

依赖:
  - GCC (MSYS2/MinGW) 在 PATH 中
  - Python 3.6+

生成:
  lfs_img.bin    — LittleFS 文件系统原始镜像 (14MB)
  batch_00.h     — ST-LINK 批烧头文件
  flash_new_batches.bat — 一键烧录脚本
"""

import os, sys, re, subprocess

# 目录
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJ_DIR   = os.path.dirname(SCRIPT_DIR)
IMG_DIR    = os.path.join(PROJ_DIR, "Core", "Src", "images")
LFS_SRC    = os.path.join(PROJ_DIR, "Core", "ThirdParty", "littlefs")
LFS_IMG    = os.path.join(SCRIPT_DIR, "lfs_img.bin")

# LittleFS 分区参数 (与 lfs_port.c 一致)
BLOCK_SIZE  = 4096
BLOCK_COUNT = 3584          # 14MB / 4KB
DISK_SIZE   = BLOCK_SIZE * BLOCK_COUNT   # 14680064 字节

# 6 张图: (文件名, LFS 文件名, 期望大小)
IMAGES = [
    ("ui_img_942215904",       "gauge.bin",     27325),
    ("ui_img_1601502596",      "needle.bin",     5676),
    ("ui_img_light_png",       "light.bin",      3072),
    ("ui_img_temp_gray_png",   "watertemp.bin",  3072),
    ("ui_img_turn_light_png",  "turnlight.bin",  3072),
    ("ui_img_safety_belt_png", "safetybelt.bin", 3072),
]

# 额外 raw 数据文件：环境变量 EXTRA_RAW = "lfs文件名,实际文件名"
# 例: EXTRA_RAW="home.bin,scripts/home_raw.bin" python 工具-从图片生成LFS镜像.py
EXTRA_RAW = os.environ.get("EXTRA_RAW", "")
if EXTRA_RAW:
    parts = EXTRA_RAW.split(",")
    if len(parts) == 2:
        lfs_name, raw_path = parts
        raw_path = os.path.join(SCRIPT_DIR, raw_path) if not os.path.isabs(raw_path) else raw_path
        if os.path.exists(raw_path):
            raw_size = os.path.getsize(raw_path)
            # (None = 从文件直接读取, LFS 文件名, 大小)
            IMAGES.append((None, lfs_name, raw_size))
            print(f"  [EXTRA] {lfs_name} <- {raw_path} ({raw_size}B)")
        else:
            print(f"  [WARN] EXTRA_RAW file not found: {raw_path}")


def extract_data(filepath, name):
    """从 LVGL .c 文件中提取 raw 像素数据"""
    with open(filepath, 'r', encoding='utf-8') as f:
        text = f.read()
    m = re.search(
        rf'const\s+(?:LV_ATTRIBUTE_MEM_ALIGN\s+)?uint8_t\s+{name}_data\[\]\s*=\s*\{{(.*?)\}};',
        text, re.DOTALL)
    if not m:
        raise ValueError(f"找不到 {name}_data 数组")
    raw = []
    for v in re.finditer(r'0x([0-9a-fA-F]{2})', m.group(1)):
        raw.append(int(v.group(1), 16))
    return bytes(raw)


def compile_mklfs():
    """用主机 GCC 编译 mklfs_host.exe"""
    src   = os.path.join(SCRIPT_DIR, "mklfs_host.c")
    lfs_c = os.path.join(LFS_SRC, "lfs.c")
    lfs_h = os.path.join(LFS_SRC, "lfs.h")
    out   = os.path.join(SCRIPT_DIR, "mklfs_host.exe")

    # 检查源文件存在
    for f in [src, lfs_c, lfs_h]:
        if not os.path.exists(f):
            print(f"ERR: 找不到 {f}")
            return None

    print(f"编译 mklfs_host.exe ...")
    result = subprocess.run(
        ["gcc", src, lfs_c, f"-I{LFS_SRC}", "-o", out, "-O2", "-Wall"],
        capture_output=True, text=True, timeout=30)

    if result.returncode != 0:
        print("编译失败:")
        print(result.stderr)
        return None

    print("  OK")
    return out


def build_lfs_image(mklfs_exe):
    """运行 mklfs_host.exe 创建 LittleFS 镜像"""
    # 从图片源文件提取数据
    file_args = []
    stdin_data = b""

    for img_name, lfs_name, expected_sz in IMAGES:
        if img_name is None:
            # 来自 EXTRA_RAW 的 raw 文件，直接读取二进制
            raw_path = os.path.join(SCRIPT_DIR, lfs_name.replace('.bin', '_raw.bin'))
            if not os.path.exists(raw_path):
                # 尝试在脚本目录查找
                raw_path = os.path.join(SCRIPT_DIR, lfs_name)
            if not os.path.exists(raw_path):
                print(f"WARN: {lfs_name} raw data not found")
                continue
            with open(raw_path, 'rb') as f:
                data = f.read()
        else:
            filepath = os.path.join(IMG_DIR, f"{img_name}.c")
            if not os.path.exists(filepath):
                print(f"ERR: 找不到 {filepath}")
                return False
            data = extract_data(filepath, img_name)

        if len(data) != expected_sz:
            print(f"WARN: {lfs_name} 大小 {len(data)} != 期望 {expected_sz}")

        # mklfs_host 根据命令行参数顺序从 stdin 读裸数据
        stdin_data += data

        file_args.extend([lfs_name, str(len(data))])
        print(f"  {lfs_name}: {len(data)}B")

    # 构造命令行
    cmd = [mklfs_exe, LFS_IMG, str(BLOCK_SIZE), str(BLOCK_COUNT)] + file_args

    print(f"\n创建 LittleFS 镜像 ({DISK_SIZE/1024/1024:.1f} MB)...")
    result = subprocess.run(cmd, input=stdin_data, capture_output=True, timeout=60)

    # mklfs_host 的 stderr 包含进度 (fprintf to stderr)
    if result.stderr:
        for line in result.stderr.decode().strip().split('\n'):
            print(f"  {line}")

    if result.returncode != 0:
        print(f"mklfs_host 失败: return code {result.returncode}")
        if result.stdout:
            print(result.stdout.decode())
        return False

    # 验证镜像大小
    actual_size = os.path.getsize(LFS_IMG)
    if actual_size != DISK_SIZE:
        print(f"WARN: 镜像大小 {actual_size} != 期望 {DISK_SIZE}")
    else:
        print(f"镜像大小: {actual_size} bytes ({actual_size/1024/1024:.1f} MB)")

    return True


def gen_openocd_script():
    """生成 OpenOCD 烧录 SPI Flash 脚本"""
    script_path = os.path.join(SCRIPT_DIR, "flash_lfs_img.tcl")
    # lfs_img.bin 写到 SPI Flash 地址 0x000000, 长度由实际文件决定
    size = os.path.getsize(LFS_IMG)
    with open(script_path, 'w') as f:
        f.write("# Auto-generated by make_lfs_image.py\n")
        f.write(f"# Flash LittleFS image to SPI Flash @ 0x000000 ({size//1024} KB)\n\n")
        f.write("init\n")
        f.write("puts \"Flash LittleFS image...\"\n")
        f.write(f"flash write_image erase lfs_img.bin 0x000000\n")
        f.write("puts \"Verify...\"\n")
        f.write(f"flash verify_image lfs_img.bin 0x000000\n")
        f.write("puts \"Done!\"\n")
        f.write("exit\n")
    print(f"\n生成 {script_path}")
    return script_path


def main():
    print("=" * 50)
    print("LittleFS 镜像生成工具")
    print("=" * 50)

    # Step 1: 编译 mklfs_host
    mklfs_exe = compile_mklfs()
    if not mklfs_exe:
        print("\n[mklfs_host 编译失败]")
        print("备选方案: 首次烧录后由嵌入式代码自动创建文件系统 (见 lfs_write_images_from_fallback)")
        print("只需确保:")
        print("  1. 编译期后备图片数据齐全 (spi_img_loader.c 中的 s_fallback)")
        print("  2. 首次启动自动格式化+写入图片文件")
        sys.exit(1)

    # Step 2: 创建 LittleFS 镜像
    if not build_lfs_image(mklfs_exe):
        sys.exit(1)

    # Step 3: 生成 OpenOCD 烧录脚本
    tcl_path = gen_openocd_script()

    print(f"\n{'='*50}")
    print("完成! 下一步:")
    print(f"  方式 A (OpenOCD, 推荐):")
    print(f"    cd {SCRIPT_DIR}")
    print(f"    openocd -s <scripts> -f <board_cfg> -f flash_lfs_img.tcl")
    print()
    print(f"  方式 B (首次启动自动创建):")
    print(f"    烧录 APP → 首次启动自动格式化 LFS + 写入图片")
    print(f"    (从编译期后备数据创建, 无需外部工具)")
    print(f"{'='*50}")


if __name__ == '__main__':
    main()
