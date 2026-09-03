#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RS485 探針 / 臨時 master（支援多位址輪詢）
====================================================================
用 USB-RS485（或任何 serial）對 bus 輪流送 REQ，並印出收到的原始 bytes。
比 Arduino 序列埠監控可靠：不 Port busy、不 NullPointerException、能看 hex、
USB 被拔會自動結束不卡死。也是 PC 上位機(模組 D) 的雛形。

用法：
    pip install pyserial          # 第一次
    python rs485_probe.py COM11              # 輪詢 addr 4
    python rs485_probe.py COM11 4,5          # 輪詢 addr 4 和 5（multidrop）
    python rs485_probe.py COM11 4,5 115200   # 指定 baud

判讀：
    addr 4 → b'4|{"t":"TMP",...}\\r'     → 該台正常回
    addr 5 → (無)                        → 該台沒回（沒接/沒燒/位址不對）
    亂碼固定不變                          → 收發器壞/接線；隨機亂 → 訊號/偏壓
"""
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    print("需要 pyserial：  pip install pyserial")
    sys.exit(1)

port = sys.argv[1] if len(sys.argv) > 1 else "COM11"
addrs = (sys.argv[2] if len(sys.argv) > 2 else "4").split(",")
baud = int(sys.argv[3]) if len(sys.argv) > 3 else 115200

print("開 %s @ %d；輪詢位址 %s（每個送 REQ|<addr>\\r），Ctrl+C 停" % (port, baud, addrs))
try:
    ser = serial.Serial(port, baud, timeout=0.3, write_timeout=1.0)
except Exception as e:
    print("開埠失敗：", e)
    print("→ 先把所有 Arduino 視窗/序列埠監控關掉（它們佔住 COM）；或 USB 重插、確認 COM 號。")
    sys.exit(1)

time.sleep(0.3)
ser.reset_input_buffer()
n = 0
alive = True
try:
    while alive:
        for a in addrs:
            n += 1
            try:
                ser.reset_input_buffer()
                ser.write(("REQ|%s\r" % a).encode())
                time.sleep(0.2)
                data = ser.read(256)
            except serial.SerialException as e:
                print("\n串口斷線/錯誤：%s → 自動結束（USB 被拔就會這樣，正常）" % e)
                alive = False
                break
            if data:
                print("[%d] addr %s → %s" % (n, a, repr(data)))
            else:
                print("[%d] addr %s → (無)" % (n, a))
        time.sleep(0.5)
except KeyboardInterrupt:
    print("\n停止")
finally:
    try:
        ser.close()
    except Exception:
        pass
