#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RS485 探針 / 臨時 master
====================================================================
用 USB-RS485（或任何 serial）對 bus 每秒送一次 REQ，並印出收到的**原始 bytes**
（同時給文字 repr 與 hex，方便判斷是乾淨資料還是亂碼/雜訊）。

比 Arduino 序列埠監控可靠：不會 Port busy、不會 NullPointerException，
還能看 hex。也是 PC 上位機(模組 D) 的雛形。

用法：
    pip install pyserial          # 第一次
    python pc/rs485_probe.py COM11            # 對 COM11 每秒送 REQ|4
    python pc/rs485_probe.py COM11 4 115200   # 指定 位址 與 baud

判讀：
    RX: b'4|{"t":"TMP",...}\\r'         → 成功！末端有回
    RX: 一堆固定怪 byte（hex 規律）      → baud 不一致
    RX: 隨機亂 byte                     → 訊號/偏壓/極性
    RX: (無)                            → 完全沒回（去程或末端問題）
"""
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    print("需要 pyserial：  pip install pyserial")
    sys.exit(1)

port = sys.argv[1] if len(sys.argv) > 1 else "COM11"
addr = sys.argv[2] if len(sys.argv) > 2 else "4"
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

print("開 %s @ %d；每秒送 REQ|%s\\r，印出收到的 bytes（Ctrl+C 停）" % (port, baud, addr))
try:
    ser = serial.Serial(port, baud, timeout=0.3)
except Exception as e:
    print("開埠失敗：", e)
    print("→ 先把所有 Arduino 視窗/序列埠監控關掉（它們佔住 COM），再跑一次。")
    sys.exit(1)

time.sleep(0.3)
ser.reset_input_buffer()
n = 0
try:
    while True:
        n += 1
        ser.write(("REQ|%s\r" % addr).encode())
        time.sleep(0.3)
        data = ser.read(256)
        if data:
            print("[%d] RX: %s   hex: %s" % (n, repr(data), data.hex(" ")))
        else:
            print("[%d] RX: (無)" % n)
        time.sleep(0.7)
except KeyboardInterrupt:
    print("\n停止")
finally:
    ser.close()
