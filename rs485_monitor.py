#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
rs485_monitor.py — 一個視窗同時監看多個序列埠（四台一起看）
====================================================================
把 中繼器 VCP、末端 VCP、USB-485 探針… 全部收進同一個視窗，
每個埠一種顏色、行首帶時間戳＋標籤，好對照「誰在什麼時間說了什麼」。
其中一個埠可設成「探針」：自動週期送 REQ|<addr>，其餘純監看（讀 debug）。

用法：
    pip install pyserial
    # 三個純監看 + 一個探針（COM10 每輪送 REQ|4、REQ|5）
    python rs485_monitor.py COM8:中繼器 COM9:末端4 COM11:末端5 COM10:探針:PROBE:4,5
    # 全部純監看（探針另外自己跑）
    python rs485_monitor.py COM8:中繼器 COM9:末端4 COM10:探針

    埠參數格式：
      COMx:標籤              → 被動監看
      COMx:標籤:PROBE:4,5    → 主動探針，輪流送 REQ|4、REQ|5 讀回覆
    選項： --baud 115200(預設)   --poll 0.5(探針每次送 REQ 間隔秒)

Ctrl+C 結束。某個埠被拔/出錯 → 只標記那一個，其他繼續。
"""
import sys, os, time, threading, argparse

try:
    import serial  # pyserial
except ImportError:
    print("需要 pyserial：  pip install pyserial")
    sys.exit(1)

# 強制 UTF-8 輸出，避免 Windows 主控台以 cp950/big5 印中文變亂碼
try:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

# Windows 開 ANSI 顏色（ENABLE_VIRTUAL_TERMINAL_PROCESSING）
if os.name == "nt":
    try:
        import ctypes
        k = ctypes.windll.kernel32
        k.SetConsoleMode(k.GetStdHandle(-11), 7)
    except Exception:
        pass

COLORS = ["\033[96m", "\033[92m", "\033[93m", "\033[95m", "\033[94m", "\033[91m"]  # 亮青綠黃紫藍紅
RESET, DIM, BOLD = "\033[0m", "\033[2m", "\033[1m"
_lock = threading.Lock()


def emit(color, label, text):
    now = time.time()
    ts = time.strftime("%H:%M:%S", time.localtime(now)) + ".%03d" % (int(now * 1000) % 1000)
    line = f"{DIM}{ts}{RESET} {color}{BOLD}{label:<7}{RESET}{color}│ {text}{RESET}"
    with _lock:
        print(line)


class Port(threading.Thread):
    def __init__(self, com, label, color, baud, probe_addrs, poll):
        super().__init__(daemon=True)
        self.com, self.label, self.color = com, label, color
        self.baud, self.probe_addrs, self.poll = baud, probe_addrs, poll
        self.buf = b""

    def run(self):
        try:
            ser = serial.Serial(self.com, self.baud, timeout=0.15, write_timeout=1.0)
        except Exception as e:
            emit(self.color, self.label, "!! 開埠失敗 %s：%s（其他埠繼續）" % (self.com, e))
            return
        tag = "  探針 REQ|%s" % ",".join(self.probe_addrs) if self.probe_addrs else ""
        emit(self.color, self.label, "== 開 %s @ %d%s ==" % (self.com, self.baud, tag))
        last_poll, idx = 0.0, 0
        try:
            while True:
                # 探針：週期輪流送 REQ|<addr>
                if self.probe_addrs and time.time() - last_poll >= self.poll:
                    a = self.probe_addrs[idx % len(self.probe_addrs)]
                    idx, last_poll = idx + 1, time.time()
                    try:
                        ser.write(("REQ|%s\r" % a).encode())
                    except Exception as e:
                        emit(self.color, self.label, "!! 寫入失敗：%s" % e)
                        break
                # 讀 + 拆行（\r 或 \n 都當一行結束）
                try:
                    data = ser.read(256)
                except Exception as e:
                    emit(self.color, self.label, "!! 斷線：%s（其他埠繼續）" % e)
                    break
                if data:
                    self.buf += data
                    while True:
                        cuts = [p for p in (self.buf.find(b"\r"), self.buf.find(b"\n")) if p >= 0]
                        if not cuts:
                            break
                        i = min(cuts)
                        line, self.buf = self.buf[:i], self.buf[i + 1:]
                        if line:
                            emit(self.color, self.label, line.decode("utf-8", "replace"))
                    if len(self.buf) > 4096:      # 防呆：一直沒換行就清掉
                        self.buf = b""
        finally:
            try:
                ser.close()
            except Exception:
                pass


def parse_arg(arg):
    parts = arg.split(":")
    com = parts[0]
    label = parts[1] if len(parts) > 1 else parts[0]
    probe = parts[3].split(",") if len(parts) >= 4 and parts[2].upper() == "PROBE" else None
    return com, label, probe


def main():
    ap = argparse.ArgumentParser(description="多埠序列監看")
    ap.add_argument("ports", nargs="+", help="COMx:標籤 或 COMx:標籤:PROBE:4,5")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--poll", type=float, default=0.5, help="探針送 REQ 間隔秒")
    a = ap.parse_args()

    threads = []
    print("%s多埠監看%s（Ctrl+C 結束）" % (BOLD, RESET))
    for i, arg in enumerate(a.ports):
        com, label, probe = parse_arg(arg)
        color = COLORS[i % len(COLORS)]
        role = "探針" if probe else "監看"
        print("  %s%-7s%s %s  %s%s" % (color, label, RESET, com, role,
                                        "  REQ|" + ",".join(probe) if probe else ""))
        threads.append(Port(com, label, color, a.baud, probe, a.poll))
    print()
    for t in threads:
        t.start()
    try:
        while any(t.is_alive() for t in threads):
            time.sleep(0.3)
        print("\n所有埠都結束了。")
    except KeyboardInterrupt:
        print("\n停止")


if __name__ == "__main__":
    main()
