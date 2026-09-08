# RS485_C 中繼器 (MicroPython / RP2040) — 模組 B
# ====================================================================
# 由 firmware/repeater_micropython/main.py（Nucleo 版）移植，行為一致，
# 只換 UART 腳位（RP2040 的 UART0/UART1）＋ 抽出「每片板子改這裡」的設定區。
#
# 這條深鏈拓撲（1-2-3-7）：
#   PC ─busU─ [R1 addr=1] ─busM─ [R2 addr=2] ─busL─ [R3 addr=3] ─busXL─ [T7 addr=7]
#   三台中繼器燒同一支 main.py，只改下面 MY_ADDR（1/2/3）。CHILDREN 三台都是 [7]。
#
# UART（三台接法一致，方便互換）：
#   down = UART(0)  GP0=TX / GP1=RX → 下層 bus（對孩子當 master，主動輪詢）
#   up   = UART(1)  GP4=TX / GP5=RX → 上層 bus（對父/PC 當 slave，被要求才回）
#   除錯 print → USB REPL（Thonny 那個 COM），跟 C++ 版 DBG 一樣
#   ⚠️ GP0/1、GP4/5 避開 W5500(GP16~21)，ETH 板也能直接用、不撞。
#
# 收發器：MAX13487（AutoDirection，免 DE/RE）→ 不需要方向控制碼。
#   之後若換 MAX13485（手動 DE/RE），在 up.write()/down.write() 前後補「拉DE→寫→等排空→放DE」
#   即可（見檔尾註解），其餘邏輯不動。
#
# 需與 lib/protocol.py 一起上傳到板子（跟 main.py 同層）。baud 必須跟末端一致（38400）。

import time
from machine import UART, Pin
import protocol as P

# ========================= 每片板子改這裡 =========================
MY_ADDR  = 1            # 這條鏈：R1=1、R2=2、R3=3（燒每片前只改這一個數字）
CHILDREN = [7]          # 這條鏈三台都是 [7]（底下唯一葉子是 T7=addr7）
BAUD     = 38400
# ================================================================

POLL_TIMEOUT_MS = 200

down = UART(0, baudrate=BAUD, tx=Pin(0), rx=Pin(1), timeout=0, timeout_char=2)  # 下層
up   = UART(1, baudrate=BAUD, tx=Pin(4), rx=Pin(5), timeout=0, timeout_char=2)  # 上層

# cache: addr -> {"path","json","ts","fresh","valid"}
cache = {}

# 診斷計數（對齊 monitor 看得懂的格式）
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
                            "path": P.addr_prefix(MY_ADDR, caddr),   # "7"->"1-7"、"3-7"->"2-3-7"
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


print("[repeater-rp2040] up, addr=%d, children=%s, baud=%d" % (MY_ADDR, CHILDREN, BAUD))
_diag = time.ticks_ms()
while True:
    for a in CHILDREN:
        poll_child(a)
        serve_upstream()
    if time.ticks_diff(time.ticks_ms(), _diag) > 1000:
        _diag = time.ticks_ms()
        print('[repeater] up_rx=%d down_rx=%d frames=%d last="%s"  cache[0]=%s'
              % (up_rx, down_rx, frames, last_frame, _cache0_status()))


# ── 之後換 MAX13485（手動 DE/RE）時：────────────────────────────
# from machine import Pin
# de_down = Pin(2, Pin.OUT, value=0)   # 下層 DE，預設低=收
# de_up   = Pin(3, Pin.OUT, value=0)   # 上層 DE，預設低=收
# def _send(uart, de, s):
#     de.value(1)
#     uart.write(s)
#     time.sleep_us(len(s) * 10_000_000 // BAUD + 100)  # 等排空(每byte≈10bit)
#     de.value(0)
# 把 up.write(...) / down.write(...) 換成 _send(up, de_up, ...) / _send(down, de_down, ...)
