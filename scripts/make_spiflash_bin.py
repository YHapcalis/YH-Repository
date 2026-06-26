#!/usr/bin/env python3
"""
从 GUI-Guider 生成的 C 数组图片文件中提取像素数据, 拼成 SPI Flash 二进制镜像。
每个图片前插入 12 字节 LVGL v9.3 lv_image_header_t, 使其可直接被 LVGL FS 解码。

用法: python make_spiflash_bin.py <图片目录> [输出文件]
输出: spiflash.bin + spiflash_offset_table.h
"""

import sys, os, re, struct, glob

# LVGL v9.3 颜色格式常量
LV_CF_MAP = {
    'LV_COLOR_FORMAT_RGB565':        0x12,
    'LV_COLOR_FORMAT_RGB565A8':      0x14,
    'LV_COLOR_FORMAT_ARGB8888':      0x08,
    'LV_COLOR_FORMAT_XRGB8888':      0x07,
    'LV_COLOR_FORMAT_NATIVE':        0x12,
    'LV_COLOR_FORMAT_NATIVE_WITH_ALPHA': 0x14,
    'LV_COLOR_FORMAT_RAW':           0x10,
}

LV_IMAGE_HEADER_MAGIC = 0x19
HEADER_SIZE = 12

def cf_str_to_int(cf_str):
    """将 cf 字段转为整数: 宏名如 LV_COLOR_FORMAT_RGB565A8, 或数值如 0x14"""
    if cf_str in LV_CF_MAP:
        return LV_CF_MAP[cf_str]
    try:
        return int(cf_str, 16 if cf_str.startswith('0x') else 10)
    except ValueError:
        return 0x12  # 默认 RGB565

def calc_stride(w, cf_int):
    """根据实际 Planar 布局计算 stride (仅颜色部分).
       RGB565A8 planar: 颜色 w×2 + Alpha w×1, stride=w×2
    """
    if cf_int == 0x14:         # RGB565A8 planar
        return w * 2
    elif cf_int in (0x08, 0x07): # ARGB8888, XRGB8888
        return w * 4
    else:                        # RGB565, RAW
        return w * 2

def parse_c_array(filepath):
    with open(filepath, encoding='utf-8', errors='ignore') as f:
        content = f.read()
    m = re.search(r'(\w+_map)\[', content)
    if not m: m = re.search(r'(\w+_data)\[', content)
    if not m: return None
    name = m.group(1)
    m = re.search(r'_map\[\]\s*=\s*\{', content)
    if not m: return None
    start = m.end() - 1
    depth, end = 0, start
    for i in range(start, len(content)):
        if content[i] == '{': depth += 1
        elif content[i] == '}':
            depth -= 1
            if depth == 0: end = i + 1; break
    tokens = re.findall(r'0x[0-9a-fA-F]+|\b[0-9A-F]{2}\b', content[start:end])
    data = bytes(int(t, 16) for t in tokens)
    wm = re.search(r'\.header\.w\s*=\s*(\d+)', content)
    hm = re.search(r'\.header\.h\s*=\s*(\d+)', content)
    cfm = re.search(r'\.header\.cf\s*=\s*(\w+)', content)
    w = int(wm.group(1)) if wm else 0
    h = int(hm.group(1)) if hm else 0
    cf = cfm.group(1) if cfm else "?"
    return (name, data, w, h, cf)

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    img_dir = sys.argv[1]
    out_bin = sys.argv[2] if len(sys.argv) > 2 else "spiflash.bin"

    files = sorted(glob.glob(os.path.join(img_dir, "*.c")))
    print(f"Found {len(files)} images")

    offset = 0
    entries = []
    with open(out_bin, 'wb') as fbin:
        for fpath in files:
            r = parse_c_array(fpath)
            if r is None: continue
            name, data, w, h, cf_str = r
            cf_int = cf_str_to_int(cf_str)
            stride = calc_stride(w, cf_int)
            size = len(data)

            # 写 LVGL 图像头部 (12 字节, little-endian)
            header = struct.pack('<BBH H H H H',
                LV_IMAGE_HEADER_MAGIC,  # magic       [0]
                cf_int,                 # cf          [1]
                0,                      # flags       [2:4]
                w,                      # width       [4:6]
                h,                      # height      [6:8]
                stride,                 # stride      [8:10]
                0                       # reserved_2  [10:12]
            )
            fbin.write(header)
            fbin.write(data)
            aligned = (size + 3) & ~3
            if aligned > size:
                fbin.write(b'\x00' * (aligned - size))

            total = HEADER_SIZE + aligned
            entries.append((name, offset, total, w, h, cf_int, stride))
            print(f"  {name:40s} {w:4d}x{h:<4d} cf=0x{cf_int:02X} {size:>8d}B"
                  f" → {total}B @ 0x{offset:08X}")
            offset += total

    print(f"\nTotal: {offset:,} bytes → {out_bin}")

    # 生成偏移表头文件
    hdr = os.path.splitext(out_bin)[0] + "_offset_table.h"
    with open(hdr, 'w') as fh:
        fh.write("// SPI Flash 图片偏移表 — 由 make_spiflash_bin.py 自动生成 (含 12B 头)\n\n")
        fh.write("#ifndef __SPIFLASH_OFFSET_TABLE_H\n#define __SPIFLASH_OFFSET_TABLE_H\n\n")
        for name, off, sz, w, h, cf_int, stride in entries:
            fh.write(f"// {name}: {w}x{h} cf=0x{cf_int:02X} stride={stride}\n")
            fh.write(f'#define OFFSET_{name.upper()}  0x{off:08X}U\n')
            fh.write(f'#define SIZE_{name.upper()}    {sz}U\n\n')
        fh.write("#endif\n")

    print(f"\n偏移表: {hdr}")

if __name__ == '__main__':
    main()
