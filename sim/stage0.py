#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RS485_C — Stage 0 模擬（一主一末端，走真實 lib/protocol.py）
====================================================================
對應硬體 Stage 0 的兩支程式：
  firmware/terminal_arduino/terminal_arduino.ino   (UNO 末端)
  firmware/stage0_esp32_master/stage0_esp32_master.ino (ESP32 主端)

不需要任何硬體：在 PC 上把「主端每秒 REQ|4 輪詢、末端回 <位址>|<json>\\r、
逾時標 stale」整條跑一遍，並印出兩塊板序列埠監控會出現的內容。
框架 encode / FrameDecoder / parse_payload 全部呼叫 lib（模組 C 定版），
所以這支模擬「就是」硬體 Stage 0 的軟體等價物。

用法： python sim/stage0.py
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lib"))
import protocol as P   # noqa: E402


# ------------------------------------------------------- 末端 (對應 UNO)
class Terminal:
    def __init__(self, addr=4):
        self.addr = addr
        self.t = 25.0          # 對應 readTempC() 的 static 起始值
        self.online = True     # False = TX 線被拔：仍在量測，但送不出
        self.mon = []          # 這塊板自己的序列埠監控
        self.mon.append("[terminal] up, addr=%d, mode=NORMAL" % self.addr)

    def read_temp_c(self):
        self.t += 0.1
        if self.t > 30.0:
            self.t = 25.0
        return self.t

    def on_request(self, req):
        """收到主端請求字串（REQ|<addr>），命中自己就回一筆框架 bytes。"""
        cmd, _, arg = req.partition("|")
        if cmd != "REQ":
            return b""
        if arg != "ALL" and arg != str(self.addr):
            return b""                                  # 不是點我 → 不回
        t = self.read_temp_c()                          # 每次被點就量一次（即使離線也量）
        js = '{"t":"TMP","v":%.1f,"u":"C","ok":1}' % t
        self.mon.append("[terminal] replied: %d|%s" % (self.addr, js))
        if not self.online:
            return b""                                  # TX 斷：有回但送不到主端
        return P.encode(str(self.addr), js, final=True)  # 單筆，lib 結尾 \r


# ------------------------------------------------------- 主端 (對應 ESP32)
class Master:
    def __init__(self, children):
        self.children = children
        self.mon = ["[master] up"]

    def poll_one(self, addr, terminal):
        """發 REQ|<addr>，逐 byte 餵 FrameDecoder，收到 \\r 的完整一筆就回，否則逾時。"""
        raw = terminal.on_request("REQ|%d" % addr)
        dec = P.FrameDecoder()
        for b in raw:
            rec = dec.push(b)
            if rec is not None:
                payload, final = rec
                caddr, cjson, _ = P.parse_payload(payload)
                return "%s|%s" % (caddr, cjson)
        return None                                     # 逾時 → stale

    def loop_once(self, terminal):
        for addr in self.children:
            data = self.poll_one(addr, terminal)
            if data is not None:
                self.mon.append("[master] addr %d -> %s" % (addr, data))
            else:
                self.mon.append("[master] addr %d -> TIMEOUT (stale)" % addr)


# ------------------------------------------------------- 跑一段情境
def main():
    term = Terminal(addr=4)
    mast = Master(children=[4])
    events = {}   # poll index -> 說明

    POLLS = 12
    for i in range(POLLS):
        if i == 6:
            term.online = False
            events[i] = "★ 拔掉 UNO 的 TX 線(D11) → 主端收不到"
        if i == 9:
            term.online = True
            events[i] = "★ TX 線接回 → 恢復"
        mast.loop_once(term)

    print("=" * 60)
    print(" ESP32 序列埠監控（主端 / master, 115200）")
    print("=" * 60)
    print(mast.mon[0])
    for i in range(POLLS):
        if i in events:
            print("        " + events[i])
        print(mast.mon[1 + i])

    print()
    print("=" * 60)
    print(" Arduino UNO 序列埠監控（末端 / terminal, 115200）")
    print("=" * 60)
    for line in term.mon:
        print(line)

    print()
    print("說明：")
    print("  - encode / FrameDecoder / parse_payload 全走 lib/protocol.py（模組 C 定版）。")
    print("  - \\r 收尾 → 主端當一筆完整資料；等不到 → 200ms 後 TIMEOUT(stale)，不卡死。")
    print("  - 拔線那 3 拍：末端自己還在量(25.7~25.9)，只是送不到；接回主端從 26.0 續 →")
    print("    主端只保證「拿到的是完整一筆」，不保證沒漏拍（要不要補序號 Stage1 再談）。")


if __name__ == "__main__":
    main()
