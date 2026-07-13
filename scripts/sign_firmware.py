#!/usr/bin/env python3
"""
sign_firmware.py — 固件签名工具

用法:
  python sign_firmware.py <input.bin> [output_dir]

生成:
  <output_dir>/signed.bin      — 原始 .bin + 40 字节签名尾部 (供 OTA)
  <output_dir>/signature.bin   — 仅 40 字节签名块 (供 OpenOCD 烧录到 0x080DFF80)

HMAC-SHA256 密钥必须与 Bootloader (sha256.c) 中的 s_sig_key 一致。
"""

import hashlib, hmac, struct, sys, os

# ── HMAC 密钥 (必须与 sha256.c 中的 s_sig_key 一致) ──
SIG_KEY = bytes([
    0x7d, 0x2e, 0x1b, 0x8f, 0x4a, 0xc3, 0x95, 0x61,
    0x0a, 0xe7, 0x52, 0xf8, 0x3c, 0xd9, 0x64, 0x10,
    0xbf, 0x85, 0x23, 0xe1, 0x79, 0x0b, 0x4e, 0xfa,
    0x16, 0xc8, 0x3d, 0xa5, 0x72, 0x90, 0xed, 0x5b,
])

SIG_MAGIC = 0xA5A5A5A5
SIG_SIZE = 40  # fw_size:4 + hmac:32 + magic:4
SIG_ADDR = 0x080DFF80  # 签名块在 Flash 中的地址


def sign_firmware(input_bin: str, output_dir: str):
    """签名固件"""
    # 读取原始 .bin
    with open(input_bin, 'rb') as f:
        fw_data = f.read()

    fw_size = len(fw_data)
    print(f"固件大小: {fw_size} bytes ({fw_size/1024:.1f} KB)")

    # 计算 HMAC-SHA256
    sig = hmac.new(SIG_KEY, fw_data, hashlib.sha256).digest()
    print(f"HMAC-SHA256: {sig.hex()}")

    # 构建签名块 (40 字节)
    sig_block = struct.pack('<I', fw_size)     # 固件大小 (4B LE)
    sig_block += sig                            # HMAC (32B)
    sig_block += struct.pack('<I', SIG_MAGIC)   # Magic (4B LE)

    assert len(sig_block) == SIG_SIZE, f"签名块大小错误: {len(sig_block)}"

    # ── 输出 signed.bin (原始数据 + 签名尾部, 供 OTA) ──
    signed_path = os.path.join(output_dir, 'signed.bin')
    with open(signed_path, 'wb') as f:
        f.write(fw_data)
        f.write(sig_block)
    print(f"signed.bin:    {signed_path}  ({fw_size + SIG_SIZE} bytes)")

    # ── 输出 signature.bin (仅签名块, 供 OpenOCD 烧录到 0x080DFF80) ──
    sig_path = os.path.join(output_dir, 'signature.bin')
    with open(sig_path, 'wb') as f:
        f.write(sig_block)
    print(f"signature.bin: {sig_path}  ({SIG_SIZE} bytes @ 0x{SIG_ADDR:08X})")

    print(f"\n烧录命令:")
    print(f"  & $openocd ... -c \"program build/Debug/MY_OTA_GUI.elf verify; "
          f"program {sig_path} {hex(SIG_ADDR)} verify; "
          f"program param_init.bin 0x080E0000 verify; exit\"")
    return 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    input_bin = sys.argv[1]
    if not os.path.exists(input_bin):
        print(f"错误: 找不到 {input_bin}")
        return 1

    output_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(input_bin)
    os.makedirs(output_dir, exist_ok=True)

    return sign_firmware(input_bin, output_dir)


if __name__ == '__main__':
    sys.exit(main())
