# RS485_C 中繼器 (MicroPython) — 模組 B v0.1
# ====================================================================
# 平台：STM32（Nucleo-F401RE 起步）。用共用協定庫 lib/protocol.py。
# 角色：對下層是 master（round-robin 輪詢 + cache），對上層是 slave（被要求才回）。
#
# 快取策略：hold-last-value + 時戳 —— 某台這輪逾時就「保留上一筆舊值」繼續給，
#           並在回報時附 age_ms 讓上層知道新舊（見 docs/06 §K）。
#
# 燒錄前：把 lib/protocol.py 複製到板子的檔案系統（與 main.py 同層）。
#
# UART：
#   down = UART(1)  下層 bus（接 MAX13487，半雙工）
#   up   = UART(2)  上層。第1層中繼器：USART2 = ST-Link VCP → 直接對 PC（免收發器）；
#                        第2層以下：UART(2) 接 MAX13487 對上層 bus。
#   （Nucleo-F401RE: USART1=PA9/PA10、USART2=PA2/PA3(VCP)。實機請對照你的板。）

import time
from machine import UART
import protocol as P

MY_ADDR = 1                 # 本台在「上層 bus」的本地位址（實機讀指撥，先寫死）
CHILDREN = [4, 5]           # 下層設備的本地位址清單
POLL_TIMEOUT_MS = 200
BAUD = 115200

down = UART(1, baudrate=BAUD, timeout=5)
up   = UART(2, baudrate=BAUD, timeout=10)

# cache: local_addr -> {"path": str, "json": str|None, "ts": ms, "fresh": bool}
cache = {}


def _read_one_record(uart, timeout_ms):
    """逐 byte 餵 FrameDecoder，收到一筆完整(\\r)記錄回 payload 字串；逾時回 None。"""
    dec = P.FrameDecoder()
    t0 = time.ticks_ms()
    while time.ticks_diff(time.ticks_ms(), t0) < timeout_ms:
        b = uart.read(1)
        if b:
            r = dec.push(b[0])
            if r is not None:
                payload, _final = r
                return payload
            t0 = time.ticks_ms()          # 有在收就續命
    return None


def poll_child(addr):
    """對某位址發 REQ、收一筆、前綴自己的位址。成功回 (path, json)；逾時回 None。"""
    while down.any():
        down.read()                       # 清殘留
    down.write("REQ|%d\r" % addr)
    payload = _read_one_record(down, POLL_TIMEOUT_MS)
    if payload is None:
        return None
    try:
        caddr, cjson, _ = P.parse_payload(payload)
    except Exception:
        return None
    return P.addr_prefix(MY_ADDR, caddr), cjson   # 逐跳前綴：32 + "4" -> "32-4"


def poll_round():
    """round-robin 輪詢下層，更新 cache（hold-last-value）。每問完一個就服務上層一次。"""
    for a in CHILDREN:
        res = poll_child(a)
        now = time.ticks_ms()
        if res is not None:
            path, cjson = res
            cache[a] = {"path": path, "json": cjson, "ts": now, "fresh": True}
        elif a in cache:
            cache[a]["fresh"] = False      # 逾時：保留舊值、標 stale
        else:
            cache[a] = {"path": "%d-%d" % (MY_ADDR, a), "json": None, "ts": now, "fresh": False}
        serve_upstream()                   # 上層優先：不必等整圈跑完


def _with_age(json_str, ts):
    """把 age_ms（距上次更新的毫秒）塞進 minified json，讓上層判斷新鮮度。"""
    age = time.ticks_diff(time.ticks_ms(), ts)
    if json_str and json_str.endswith("}"):
        return json_str[:-1] + ',"age":%d}' % age
    return json_str


def serve_upstream():
    """上層要資料：REQ|<addr> 回該筆、REQ|ALL 回全部（各自以 \\r 結尾）。一律撈 cache。"""
    if not up.any():
        return
    line = up.readline()
    if not line:
        return
    try:
        req = line.decode().strip()
    except Exception:
        return
    if not req.startswith("REQ|"):
        return
    arg = req[4:]
    if arg == "ALL":
        targets = CHILDREN
    elif arg.isdigit():
        targets = [int(arg)]
    else:
        targets = []
    for a in targets:
        e = cache.get(a)
        if e and e["json"]:
            up.write("%s|%s\r" % (e["path"], _with_age(e["json"], e["ts"])))
        # 無有效值就不回該筆（上層自行逾時處理）


print("[repeater] up, addr=%d, children=%s" % (MY_ADDR, CHILDREN))
while True:
    poll_round()
