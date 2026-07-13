#!/usr/bin/env python3
"""
wifi_ota_send.py — WiFi OTA 固件推送工具 (双模式)

开发模式 (定向推送):
    python wifi_ota_send.py <ESP8266_IP> <firmware.bin>
    python wifi_ota_send.py <ESP8266_IP> <firmware.bin> --who          # UID 握手确认
    python wifi_ota_send.py <ESP8266_IP> <firmware.bin> --who --target-uid <UID>  # 校验目标板

量产模式 (UDP广播触发, 各板反连TCP下载):
    python wifi_ota_send.py <firmware.bin> --broadcast                  # 自动广播+下载
    python wifi_ota_send.py <firmware.bin> --broadcast --port 8765      # 自定下载端口

示例:
    python wifi_ota_send.py 192.168.0.235 build/Debug/MY_OTA_GUI.bin
    python wifi_ota_send.py build/Debug/MY_OTA_GUI.bin --broadcast

流程:
    定向:  PC → TCP:8080 → ESP8266 → USART3 → F407 → SPI Flash → 复位 → Bootloader
    广播:  PC → UDP:8081 (OTA:size:PC_IP:port) → 各板连 PC:8765 (TCP下载) → 各自更新
"""

import socket, sys, os, time, argparse

TCP_PORT = 8080          # 定向: TCP Server 端口 (F407 ESP8266 监听)
# unused now 广播: UDP 监听端口 (F407 ESP8266 监听)
DL_PORT  = 8765          # 广播: 固件下载 TCP 端口 (PC 临时 Server)
CHUNK_SIZE = 128         # 每块字节数 (ESP8266 缓存安全值)
SEND_DELAY = 0.02        # 块间延迟


def get_local_ip():
    """获取本机局域网 IP"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
    except Exception:
        ip = "127.0.0.1"
    finally:
        s.close()
    return ip


def who_handshake(sock, target_uid=None):
    """WHO 握手: 发送 WHO 命令, 读 UID, 可选校验"""
    sock.sendall(b"WHO\r\n")
    time.sleep(0.3)
    try:
        resp = sock.recv(100).decode(errors="replace").strip()
    except socket.timeout:
        print("[WARN] WHO: no response")
        return True  # 旧固件不支持 WHO, 跳过

    print(f"[WHO] Board reply: {resp}")
    if target_uid:
        if target_uid in resp:
            print(f"[WHO] UID match! Sending firmware...")
            return True
        else:
            print(f"[ERR] Wrong board! Got {resp}, expected UID containing {target_uid}")
            return False
    return True


def directed_mode(ip, bin_path, do_who, target_uid):
    """定向推送: TCP 连接到指定 ESP8266"""
    if not os.path.isfile(bin_path):
        print(f"[ERR] File not found: {bin_path}")
        return False
    fw_size = os.path.getsize(bin_path)
    if fw_size < 4096 or fw_size > 0x100000:
        print(f"[ERR] Invalid size: {fw_size}")
        return False

    print(f"[DIRECTED] Target: {ip}:{TCP_PORT}")
    print(f"[DIRECTED] Firmware: {bin_path} ({fw_size} bytes)")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5)
    try:
        sock.connect((ip, TCP_PORT))
        print("[DIRECTED] Connected!")
    except Exception as e:
        print(f"[ERR] Connection failed: {e}")
        sock.close()
        return False

    # WHO 握手
    if do_who:
        if not who_handshake(sock, target_uid):
            sock.close()
            return False

    # 发送 OTA 起始命令
    cmd = f"OTA:{fw_size}\r\n"
    sock.sendall(cmd.encode())
    print(f"[OTA] Sent: {cmd.strip()}")
    time.sleep(0.5)

    # 分块发送
    sent_total = 0
    last_pct = -1
    t0 = time.time()
    with open(bin_path, "rb") as f:
        while True:
            chunk = f.read(CHUNK_SIZE)
            if not chunk:
                break
            sock.sendall(chunk)
            sent_total += len(chunk)
            pct = (sent_total * 100) // fw_size
            if pct != last_pct and pct % 10 == 0:
                elapsed = time.time() - t0
                speed = sent_total / elapsed / 1024 if elapsed > 0 else 0
                print(f"[OTA] {pct}% ({sent_total}/{fw_size}, {speed:.1f} KB/s)")
                last_pct = pct
            time.sleep(SEND_DELAY)

    elapsed = time.time() - t0
    speed = fw_size / elapsed / 1024
    print(f"\n[OTA] Done! {sent_total} bytes in {elapsed:.1f}s ({speed:.1f} KB/s)")
    time.sleep(2)
    sock.close()
    print("[OTA] Board should reboot into bootloader now.")
    return True


def broadcast_mode(bin_path, dl_port):
    """广播推送: 顺序遍历每块板, 逐一定向 OTA (复用已验证的路径)"""
    if not os.path.isfile(bin_path):
        print(f"[ERR] File not found: {bin_path}")
        return False
    fw_size = os.path.getsize(bin_path)
    if fw_size < 4096 or fw_size > 0x100000:
        print(f"[ERR] Invalid size: {fw_size}")
        return False

    local_ip = get_local_ip()
    subnet = ".".join(local_ip.split(".")[:3])

    # 扫描子网找板子
    boards = []
    print(f"[BROADCAST] Scanning {subnet}.x:8080 for boards...")
    for i in range(1, 255):
        try:
            s = socket.socket()
            s.settimeout(0.1)
            s.connect((f"{subnet}.{i}", TCP_PORT))
            boards.append(f"{subnet}.{i}")
            s.close()
        except:
            pass
    print(f"[BROADCAST] Found {len(boards)} board(s): {boards}")

    if not boards:
        print("[BROADCAST] No boards found!")
        return False

    # 逐板定向 OTA 推送
    success = 0
    total_time = 0
    for idx, ip in enumerate(boards):
        print(f"\n[BROADCAST] --- Board {idx+1}/{len(boards)}: {ip} ---")
        t0 = time.time()
        ok = directed_mode(ip, bin_path, do_who=False, target_uid=None)
        elapsed = time.time() - t0
        total_time += elapsed
        if ok:
            success += 1
            print(f"[BROADCAST] Board {ip} done ({elapsed:.0f}s)")
            # 等板子重启 + Bootloader 恢复 + APP 启动 + WiFi 重连
            wait = 20
            print(f"[BROADCAST] Waiting {wait}s for next board...")
            time.sleep(wait)

    print(f"\n[BROADCAST] Done! {success}/{len(boards)} board(s) updated in {total_time:.0f}s")
    return success > 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="WiFi OTA firmware sender")
    parser.add_argument("target", help="ESP8266 IP or firmware.bin for broadcast")
    parser.add_argument("firmware", nargs="?", help="firmware .bin file")
    parser.add_argument("--who", action="store_true", help="WHO handshake (directed)")
    parser.add_argument("--target-uid", help="expected UID substring to verify")
    parser.add_argument("--broadcast", action="store_true", help="broadcast mode")
    parser.add_argument("--port", type=int, default=DL_PORT, help="download port")

    args = parser.parse_args()

    if args.broadcast:
        # 广播模式: target 参数是固件路径
        success = broadcast_mode(args.target, args.port)
    else:
        # 定向模式: target=IP, firmware 是第二个参数
        if not args.firmware:
            parser.print_help()
            sys.exit(1)
        success = directed_mode(args.target, args.firmware,
                                args.who, args.target_uid)

    sys.exit(0 if success else 1)
