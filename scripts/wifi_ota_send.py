#!/usr/bin/env python3
"""
wifi_ota_send.py — 通过 WiFi (TCP) 向 F407 ESP8266 发送固件

用法:
    python wifi_ota_send.py <ESP8266_IP> <firmware.bin>

示例:
    python wifi_ota_send.py 192.168.1.100 build/Debug/MY_OTA_GUI.bin

流程:
    PC → TCP:8080 → ESP8266 → USART3 → F407 → SPI Flash → 复位 → Bootloader → APP
"""

import socket
import sys
import os
import time

TCP_PORT = 8080
CHUNK_SIZE = 128       # 每块字节数 (ESP8266 缓存安全值)
SEND_DELAY = 0.02      # 块间延迟 (秒)
TAIL_CRLF  = b"\r\n"   # ESP8266 的 +IPD 帧分隔符


def send_firmware(ip: str, bin_path: str) -> bool:
    """通过 TCP 向 ESP8266 发送固件"""

    # ── 读取固件文件 ──
    if not os.path.isfile(bin_path):
        print(f"[ERR] File not found: {bin_path}")
        return False

    fw_size = os.path.getsize(bin_path)
    if fw_size < 4096 or fw_size > 0x100000:
        print(f"[ERR] Invalid firmware size: {fw_size} bytes (expected 4KB~1MB)")
        return False

    print(f"[OTA] Firmware: {bin_path}")
    print(f"[OTA] Size:     {fw_size} bytes ({fw_size/1024:.1f} KB)")
    print(f"[OTA] Target:   {ip}:{TCP_PORT}")
    print()

    # ── 连接 TCP Server ──
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    try:
        sock.connect((ip, TCP_PORT))
        print("[OTA] Connected!")
    except Exception as e:
        print(f"[ERR] Connection failed: {e}")
        sock.close()
        return False

    # ── 发送 OTA 起始命令 ──
    cmd = f"OTA:{fw_size}\r\n"
    sock.sendall(cmd.encode())
    print(f"[OTA] Sent: {cmd.strip()}")
    time.sleep(0.5)

    # ── 分块发送固件 ──
    sent_total = 0
    last_progress = -1
    start_time = time.time()

    with open(bin_path, "rb") as f:
        while True:
            chunk = f.read(CHUNK_SIZE)
            if not chunk:
                break

            sock.sendall(chunk)
            sent_total += len(chunk)

            # 进度显示
            progress = (sent_total * 100) // fw_size
            if progress != last_progress and progress % 10 == 0:
                elapsed = time.time() - start_time
                speed = sent_total / elapsed / 1024 if elapsed > 0 else 0
                print(f"[OTA] {progress}% ({sent_total}/{fw_size} bytes, {speed:.1f} KB/s)")
                last_progress = progress

            time.sleep(SEND_DELAY)

    elapsed = time.time() - start_time
    speed = fw_size / elapsed / 1024 if elapsed > 0 else 0
    print(f"\n[OTA] Done! {sent_total} bytes sent in {elapsed:.1f}s ({speed:.1f} KB/s)")

    # 等待 ESP8266 处理完成
    time.sleep(2)
    sock.close()

    print("[OTA] Connection closed. F407 should reboot into bootloader soon.")
    return True


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)

    ip = sys.argv[1]
    bin_path = sys.argv[2]

    success = send_firmware(ip, bin_path)
    sys.exit(0 if success else 1)
