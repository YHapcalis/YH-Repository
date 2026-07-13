#!/usr/bin/env python3
"""
ymodem_send.py — Ymodem 协议发送 .bin 固件 (CH340C DTR 适配版)

用法:
  python scripts/ymodem_send.py <COM口> <固件.bin>

依赖: pyserial (pip install pyserial)
"""

import sys, os, struct, time

try:
    import serial
except ImportError:
    print("需要 pyserial 库: pip install pyserial")
    sys.exit(1)

SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CA  = 0x18
CRC_CODE = 0x43

def crc16(data: bytes) -> int:
    """与 Bootloader ymodem.c 完全一致的 CRC16 算法"""
    def update_crc16(crc: int, byte: int) -> int:
        inv = byte | 0x100
        while not (inv & 0x10000):
            crc <<= 1
            inv <<= 1
            if inv & 0x100:
                crc += 1
            if crc & 0x10000:
                crc ^= 0x1021
        return crc & 0xFFFF

    crc = 0
    for b in data:
        crc = update_crc16(crc, b)
    crc = update_crc16(crc, 0)
    crc = update_crc16(crc, 0)
    return crc

def send_packet(ser, pkt_num: int, data: bytes, long_wait: bool = False) -> bool:
    size = len(data)
    pkt_size = 128 if size <= 128 else 1024
    header = SOH if size <= 128 else STX

    buf = bytearray(pkt_size)
    buf[:size] = data
    if size < pkt_size:
        buf[size:] = b'\x1A' * (pkt_size - size)

    pkt = bytearray([header, pkt_num & 0xFF, (~pkt_num) & 0xFF])
    pkt.extend(buf)
    c = crc16(buf)
    pkt.extend([c >> 8, c & 0xFF])

    for attempt in range(15):
        try:
            ser.write(pkt)
            # 头包首试用长超时（等待 Bootloader 擦除 Flash）
            t = 8.0 if (long_wait and attempt == 0) else 0.5
            saved = ser.timeout
            ser.timeout = t
            ack = ser.read(1)
            ser.timeout = saved
            if ack == b'\x06':
                # 排空后续多余字节（Bootloader 发完 ACK 后可能跟了 CRC 等）
                ser.timeout = 0.05
                while ser.read(1):
                    pass
                ser.timeout = saved
                return True
            elif ack == b'\x15':
                continue
            time.sleep(0.2)
        except:
            pass
    return False

def read_with_timeout(ser, length, timeout=2):
    """读取数据直到满足长度或超时"""
    data = b''
    end = time.time() + timeout
    while time.time() < end and len(data) < length:
        try:
            chunk = ser.read(min(length - len(data), max(1, int((end - time.time()) * 100))))
            if chunk:
                data += chunk
        except:
            break
    return data

def ymodem_send(port: str, filepath: str, baud: int = 115200):
    with open(filepath, 'rb') as f:
        file_data = f.read()

    filename = os.path.basename(filepath).encode('ascii')
    filesize = len(file_data)

    print(f"  File: {filename.decode()} ({filesize} bytes)")
    print(f"  Port: {port} @ {baud}")

    # ── 打开串口（禁止 DTR，防 CH340C 自动复位）──
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0.5
    ser.rts = False
    ser.dtr = False
    ser.open()
    # Windows 驱动 open 时可能脉冲 DTR，再补一刀确保 DTR 为低
    ser.dtr = False
    print("  Serial opened (DTR=OFF, press RESET on board)")

    # ── 等待 Bootloader 输出，直到出现 'C' (Ymodem CRC 请求) ──
    print("  Waiting for bootloader (CRC='C')...")
    print("  >>> Press RESET button on the board now <<<")
    start = time.time()
    found_c = False
    buf = b''
    while time.time() - start < 15:
        try:
            b = ser.read(1)
            if b:
                buf += b
                if b[0] == CRC_CODE:
                    found_c = True
                    break
        except:
            pass

    if not found_c:
        # 没收到 'C'，主动发一个试试
        print("  No 'C' received, sending CRC request...")
        ser.write(bytes([CRC_CODE]))
        time.sleep(2)
        for _ in range(100):
            b = ser.read(1)
            if b:
                buf += b
                if b[0] == CRC_CODE:
                    found_c = True
                    break

    if not found_c:
        print(f"  FAIL: No Ymodem CRC request (received {len(buf)} bytes)")
        # 显示收到的内容供诊断
        text = ''.join(chr(b) for b in buf if 32 <= b < 127)
        if text:
            print(f"  Captured: {text[:200]}")
        ser.close()
        return False

    print(f"  Ymodem ready (CRC detected)")
    print("  Erasing APP sectors (wait ~5s)...")

    # ── 发送文件头包 (pkt=0, filename+size) ──
    header_data = filename + b'\x00' + str(filesize).encode() + b'\x00'
    # 头包需要更长超时：Bootloader 收到后先擦除 Flash，数秒后才回 ACK
    time.sleep(1)
    if not send_packet(ser, 0, header_data, long_wait=True):
        print("  FAIL: Header packet")
        ser.close()
        return False
    print(f"  Header sent: {filename.decode()} ({filesize}B)")

    # 等待接收方回应第二个 'C'
    time.sleep(0.5)
    for _ in range(10):
        b = ser.read(1)
        if b == b'\x43':
            break

    # ── 发送数据包 ──
    pkt_num = 1
    offset = 0
    chunk_size = 1024
    total_packets = (filesize + chunk_size - 1) // chunk_size

    while offset < filesize:
        chunk = file_data[offset:offset + chunk_size]
        progress = min(offset + len(chunk), filesize)
        pct = 100 * progress // filesize
        sys.stdout.write(f"\r  Packet {pkt_num}/{total_packets} ({pct}%)     ")
        sys.stdout.flush()

        if not send_packet(ser, pkt_num, chunk):
            print(f"\n  FAIL: Packet {pkt_num}")
            ser.close()
            return False

        offset += chunk_size
        pkt_num += 1

    print(f"\r  Packet {pkt_num-1}/{total_packets} (100%) - done")

    # ── 发送 EOT ──
    time.sleep(0.3)
    ser.write(bytes([EOT]))
    ack = read_with_timeout(ser, 1, 2)
    if ack == b'\x06':
        print("  EOT ACKed")
    else:
        print(f"  EOT response: {ack.hex() if ack else 'none'}")

    # ── 结束包 ──
    time.sleep(0.3)
    ser.write(bytes([CRC_CODE]))
    time.sleep(1)
    send_packet(ser, 0, b'')

    ser.close()
    print(f"\n  OTA Done! ({filesize} bytes sent)")
    return True

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <COM> <firmware.bin>")
        print(f"  eg: {sys.argv[0]} COM3 build/Debug/MY_OTA_GUI.bin")
        sys.exit(1)
    port = sys.argv[1]
    filepath = sys.argv[2]
    if not os.path.exists(filepath):
        print(f"File not found: {filepath}")
        sys.exit(1)
    ok = ymodem_send(port, filepath)
    sys.exit(0 if ok else 1)
