# RS485_C 中繼器 (MicroPython) — 模組 B
# ====================================================================
# 對應目前跑通的 C++ 版 nucleo_repeater，行為一致，改成 MicroPython。
# 平台：Nucleo-F401RE。用共用協定庫 protocol.py（要跟 main.py 同層上傳到板子）。
#
# UART（對照目前硬體，跟 C++ 版一樣的接法）：
#   down = UART(1)  USART1 PA9/PA10 → MAX_2 下層 bus（接末端）
#   up   = UART(6)  USART6 PC6/PC7  → MAX_1 上層 bus（USB-485 ↔ PC）
#   除錯 print → REPL（USART2 = ST-Link VCP = 中繼器那個 COM），跟 C++ 的 DBG 一樣
#   ⚠️ 若板子的 UART(1)/UART(6) 預設腳不是 PA9/PA10、PC6/PC7，在 REPL 用
#      `UART(1)` / `UART(6)` 印出來對一下，或改用能指定腳位的建構式。
#
# baud 必須跟末端 + 探針一致（目前 38400）。
# 快取：hold-last-value + age（逾時保留舊值、標 stale，回報時附 age_ms）。

import time
from machine import UART
import protocol as P

MY_ADDR = 1                 # 本台在上層 bus 的本地位址（實機可改讀指撥）
CHILDREN = [4, 5]           # 下層末端的本地位址
BAUD = 38400
POLL_TIMEOUT_MS = 200

down = UART(1, baudrate=BAUD, timeout=0, timeout_char=2)   # 下層（非阻塞讀）
up   = UART(6, baudrate=BAUD, timeout=0, timeout_char=2)   # 上層

# cache: addr -> {"path","json","ts","fresh","valid"}
cache = {}

# 診斷計數（對齊 C++ 版 monitor 看得懂的格式）
up_rx = 0
down_rx = 0
frames = 0
last_frame = ""
_upbuf = bytearray()


def _decode(bs):
    try:
        return bs.decode("utf-8")
    except Exception:
        return None


def serve_slot(a):
    """把某孩子的 cache 回給上層，json 尾插 age_ms。逾時保留舊值也照回。"""
    e = cache.get(a)
    if not e or not e.get("valid"):
        return
    j = e["json"]
    if j:
        age = time.ticks_diff(time.ticks_ms(), e["ts"])
        if j.endswith("}"):
            j = j[:-1] + ',"age":%d}' % age
        up.write("%s|%s\r" % (e["path"], j))


def serve_upstream():
    """上層要資料：REQ|<addr> 回該筆、REQ|ALL 回全部。一律撈 cache。"""
    global up_rx, _upbuf
    data = up.read()
    if not data:
        return
    up_rx += len(data)
    for b in data:
        if b == 0x0D or b == 0x0A:
            req = _decode(bytes(_upbuf))
            _upbuf = bytearray()
            if req and req.startswith("REQ|"):
                arg = req[4:]
                if arg == "ALL":
                    for a in CHILDREN:
                        serve_slot(a)
                elif arg.isdigit():
                    a = int(arg)
                    if a in CHILDREN:
                        serve_slot(a)
        elif len(_upbuf) < 64:
            _upbuf.append(b)


def poll_child(a):
    """對某孩子發 REQ、收一筆完整框、前綴自己的位址存 cache。逾時保留舊值標 stale。"""
    global down_rx, frames, last_frame
    down.read()                         # flush 殘留
    down.write("REQ|%d\r" % a)
    dec = P.FrameDecoder()
    t0 = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), t0) < POLL_TIMEOUT_MS:
        serve_upstream()                # 快取解耦：等下層回覆時也持續服務上層
        data = down.read()
        if data:
            down_rx += len(data)
            for b in data:
                try:
                    r = dec.push(b)
                except Exception:       # 亂碼 byte 令 decode 失敗 → 重新同步
                    dec.reset()
                    r = None
                if r is not None:
                    frames += 1
                    payload, _final = r
                    last_frame = payload[:60]
                    try:
                        caddr, cjson, _ = P.parse_payload(payload)
                    except Exception:
                        continue
                    if P.addr_valid(caddr):     # 濾掉 REQ 回音/雜訊
                        cache[a] = {
                            "path": P.addr_prefix(MY_ADDR, caddr),   # "4" -> "1-4"
                            "json": cjson,
                            "ts": time.ticks_ms(),
                            "fresh": True, "valid": True,
                        }
                        return
            t0 = time.ticks_ms()
    if a in cache:
        cache[a]["fresh"] = False       # 逾時：保留舊值、標 stale


def _cache0_status():
    e = cache.get(CHILDREN[0])
    if not e or not e.get("valid"):
        return "(空)"
    return e["path"] + (" fresh" if e["fresh"] else " stale")


print("[repeater-mpy] up, addr=%d, children=%s, baud=%d" % (MY_ADDR, CHILDREN, BAUD))
_diag = time.ticks_ms()
while True:
    for a in CHILDREN:
        poll_child(a)
        serve_upstream()
    if time.ticks_diff(time.ticks_ms(), _diag) > 1000:
        _diag = time.ticks_ms()
        print('[repeater] up_rx=%d down_rx=%d frames=%d last="%s"  cache[0]=%s'
              % (up_rx, down_rx, frames, last_frame, _cache0_status()))
