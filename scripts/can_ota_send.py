#!/usr/bin/env python3
"""
can_ota_send.py — 通过 F103 网关向 F407 发送固件 (串口 → ISO-TP → CAN)

用法:
  python scripts/can_ota_send.py COMx build/Debug/MY_OTA_GUI.bin

流程:
  1. 打开 F103 串口 (不用 DTR 操作)
  2. 发送 .bin 裸字节流
  3. F103 自动分包 → ISO-TP → F407 Bootloader 写 Flash
"""

import sys, os, time

try:
    import serial
except ImportError:
    print("需要 pyserial: pip install pyserial")
    sys.exit(1)

def send_firmware(port: str, filepath: str, baud: int = 115200):
    with open(filepath, 'rb') as f:
        data = f.read()

    filesize = len(data)
    filename = os.path.basename(filepath)

    print(f"  File: {filename} ({filesize} bytes)")
    print(f"  Port: {port} @ {baud}")

    ser = serial.Serial(port=port, baudrate=baud, timeout=2)
    ser.dtr = False
    ser.rts = False

    # 等 F103 网关就绪
    time.sleep(1)
    ser.reset_input_buffer()

    print("  Sending firmware...")
    written = 0
    chunk_size = 2048
    start = time.time()

    for offset in range(0, filesize, chunk_size):
        chunk = data[offset:offset + chunk_size]
        ser.write(chunk)
        ser.flush()
        written += len(chunk)
        pct = 100 * written // filesize
        elapsed = time.time() - start
        speed = written / 1024 / elapsed if elapsed > 0 else 0
        print(f"\r  {pct}% ({written}/{filesize} bytes)  {speed:.1f} KB/s", end="")
        # 给 F103 留处理时间
        time.sleep(0.05)

    print(f"\n  Done! {filesize} bytes sent in {time.time()-start:.1f}s")
    ser.close()
    return True

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <COM> <firmware.bin>")
        sys.exit(1)
    ok = send_firmware(sys.argv[1], sys.argv[2])
    sys.exit(0 if ok else 1)
