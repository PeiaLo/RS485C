#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PLC 練習用：模擬「客戶的 PLC 外設」(Modbus slave)
====================================================================
不用買硬體，讓 PLC 模擬器 / CODESYS / QModMaster 當 master 來讀寫。
一支程式同時模擬兩台外設（同一條 bus、不同 slave 位址）：

  unit 1：變頻器（仿台達風格暫存器，實機請以手冊為準）
  unit 2：溫控器（加熱爐，一階慣性；變頻器帶的風扇轉越快、降溫越多）

兩種接法：
  Modbus TCP   ：python sim/plc_practice/fake_peripherals.py --tcp 5020
  Modbus RTU   ：python sim/plc_practice/fake_peripherals.py --rtu COM11 --baud 9600 --parity E
                 （需 com0com 建一對虛擬 COM，例如 COM10<->COM11，master 開 COM10）
  兩個一起開也可以。

故障注入（練逾時 / 重試 / 通訊異常處理）：
  --drop 0.1      10% 的請求不回應
  --crc-err 0.05  RTU 5% 的回應 CRC 故意算錯
自我測試：python sim/plc_practice/fake_peripherals.py --selftest

只用標準函式庫（RTU 需要 pyserial）。CRC 自己算，不依賴 lib/protocol.py。
"""
import argparse
import random
import socket
import struct
import sys
import threading
import time

# ====================================================================
# CRC-16/MODBUS
# ====================================================================
def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


# ====================================================================
# 外設模型
# ====================================================================
class Device:
    """一台 Modbus slave：holding 暫存器表 + 唯讀位址集合 + 物理模型。"""
    name = "device"

    def __init__(self):
        self.regs = {}
        self.read_only = set()
        self.lock = threading.Lock()

    def read(self, addr, count):
        with self.lock:
            if any((addr + i) not in self.regs for i in range(count)):
                return None
            return [self.regs[addr + i] for i in range(count)]

    def write(self, addr, values):
        """回傳 None=成功，否則 Modbus 例外碼。"""
        with self.lock:
            for i in range(len(values)):
                a = addr + i
                if a not in self.regs or a in self.read_only:
                    return 0x02
            for i, v in enumerate(values):
                err = self.on_write(addr + i, v)
                if err:
                    return err
                self.regs[addr + i] = v
        return None

    def on_write(self, addr, value):
        return None

    def step(self, dt, world):
        pass


class FakeVFD(Device):
    """變頻器。0x2000 命令、0x2001 頻率命令(0.01Hz)、0x21xx 狀態。"""
    name = "VFD"
    CMD, FREQ_CMD = 0x2000, 0x2001
    ERR, STATUS, FREQ_SET_ECHO, FREQ_OUT, CURRENT = 0x2100, 0x2101, 0x2102, 0x2103, 0x2104
    MAX_HZ = 60.0
    ACCEL_S = 10.0          # 0 → 60Hz 需 10 秒

    def __init__(self):
        super().__init__()
        for a in (self.CMD, self.FREQ_CMD, self.ERR, self.STATUS,
                  self.FREQ_SET_ECHO, self.FREQ_OUT, self.CURRENT):
            self.regs[a] = 0
        self.regs[self.CMD] = 0x0011          # 停止、正轉
        self.read_only = {self.ERR, self.STATUS, self.FREQ_SET_ECHO, self.FREQ_OUT, self.CURRENT}
        self.hz = 0.0

    def on_write(self, addr, value):
        if addr == self.FREQ_CMD and value > int(self.MAX_HZ * 100):
            return 0x03                       # 超過上限 → 非法數值
        if addr == self.CMD and (value & 0x3) not in (1, 2):
            return 0x03                       # bit0-1 只能 01=停 / 10=運轉
        return None

    def step(self, dt, world):
        cmd = self.regs[self.CMD]
        running = (cmd & 0x3) == 2
        reverse = ((cmd >> 4) & 0x3) == 2
        target = self.regs[self.FREQ_CMD] / 100.0 if running else 0.0
        ramp = self.MAX_HZ / self.ACCEL_S * dt
        if self.hz < target:
            self.hz = min(target, self.hz + ramp)
        else:
            self.hz = max(target, self.hz - ramp)
        amps = 0.5 + 3.5 * (self.hz / self.MAX_HZ) if self.hz > 0 else 0.0
        status = 0
        if self.hz > 0:
            status |= 0x1                     # bit0 運轉中
        if reverse:
            status |= 0x2                     # bit1 反轉
        if running and abs(self.hz - target) < 0.01:
            status |= 0x4                     # bit2 到達設定頻率
        self.regs[self.STATUS] = status
        self.regs[self.FREQ_SET_ECHO] = self.regs[self.FREQ_CMD]
        self.regs[self.FREQ_OUT] = int(round(self.hz * 100))
        self.regs[self.CURRENT] = int(round(amps * 10))
        world["fan_hz"] = self.hz


class FakeTempController(Device):
    """溫控器 + 加熱爐。PV/SV 單位 0.1°C。

    PID 參數可經 Modbus 調整（出廠 I=0、D=0 → 純 P，會有穩態誤差）。
    加熱到感測器有 DEAD_TIME 秒延遲，P 調太大會震盪，像真的機台。
    """
    name = "TEMP"
    PV, SV, OUT, RUN, ALM_HI, ALM_ST = 0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005
    KP, TI, TD = 0x1010, 0x1011, 0x1012       # P ×0.1、I 秒(0=關)、D ×0.1 秒(0=關)
    AMBIENT = 25.0
    DEAD_TIME = 3.0

    def __init__(self):
        super().__init__()
        self.regs.update({self.PV: 250, self.SV: 800, self.OUT: 0,
                          self.RUN: 0, self.ALM_HI: 1000, self.ALM_ST: 0,
                          self.KP: 80, self.TI: 0, self.TD: 0})
        self.read_only = {self.PV, self.OUT, self.ALM_ST}
        self.temp = self.AMBIENT
        self.integral = 0.0
        self.prev_err = None
        self.pipe = []                        # 延遲中的加熱量

    def on_write(self, addr, value):
        if addr in (self.SV, self.ALM_HI) and value > 3000:
            return 0x03                       # 上限 300.0°C
        if addr == self.RUN and value not in (0, 1):
            return 0x03
        if addr in (self.KP, self.TI, self.TD) and value > 9999:
            return 0x03
        return None

    def step(self, dt, world):
        sv = self.regs[self.SV] / 10.0
        out = 0.0
        if self.regs[self.RUN]:
            kp = self.regs[self.KP] / 10.0
            ti = self.regs[self.TI]
            td = self.regs[self.TD] / 10.0
            err = sv - self.temp
            deriv = 0.0 if self.prev_err is None else (err - self.prev_err) / dt
            self.prev_err = err
            i_term = kp * self.integral / ti if ti else 0.0
            raw = kp * err + i_term + kp * td * deriv
            out = max(0.0, min(100.0, raw))
            if ti and out == raw:             # 抗積分飽和：輸出飽和時不累積
                self.integral += err * dt
            elif not ti:
                self.integral = 0.0
        else:
            self.integral = 0.0
            self.prev_err = None
        self.pipe.append(out * 0.03)          # °C/s @ 100%
        heat = self.pipe.pop(0) if len(self.pipe) > self.DEAD_TIME / dt else 0.0
        loss = (self.temp - self.AMBIENT) / 120.0                  # 自然散熱
        fan = (self.temp - self.AMBIENT) * world.get("fan_hz", 0.0) / 60.0 / 15.0
        self.temp += (heat - loss - fan) * dt
        self.temp += random.uniform(-0.02, 0.02)                   # 一點雜訊
        self.regs[self.PV] = int(round(self.temp * 10)) & 0xFFFF
        self.regs[self.OUT] = int(round(out * 10))
        self.regs[self.ALM_ST] = 1 if self.regs[self.PV] >= self.regs[self.ALM_HI] else 0


# ====================================================================
# Modbus PDU 處理（RTU / TCP 共用）
# ====================================================================
def exception(fc, code):
    return bytes([fc | 0x80, code])


def handle_pdu(dev, pdu):
    if len(pdu) < 1:
        return None
    fc = pdu[0]
    try:
        if fc in (0x03, 0x04):
            addr, count = struct.unpack(">HH", pdu[1:5])
            if not 1 <= count <= 125:
                return exception(fc, 0x03)
            vals = dev.read(addr, count)
            if vals is None:
                return exception(fc, 0x02)
            return bytes([fc, count * 2]) + b"".join(struct.pack(">H", v) for v in vals)
        if fc == 0x06:
            addr, value = struct.unpack(">HH", pdu[1:5])
            err = dev.write(addr, [value])
            return exception(fc, err) if err else pdu[:5]
        if fc == 0x10:
            addr, count, nbytes = struct.unpack(">HHB", pdu[1:6])
            if not 1 <= count <= 123 or nbytes != count * 2 or len(pdu) < 6 + nbytes:
                return exception(fc, 0x03)
            values = list(struct.unpack(">%dH" % count, pdu[6:6 + nbytes]))
            err = dev.write(addr, values)
            return exception(fc, err) if err else pdu[:5]
    except struct.error:
        return exception(fc, 0x03)
    return exception(fc, 0x01)


class Bus:
    def __init__(self, devices, drop=0.0, crc_err=0.0, verbose=False):
        self.devices = devices
        self.drop = drop
        self.crc_err = crc_err
        self.verbose = verbose

    def dispatch(self, unit, pdu, tag):
        dev = self.devices.get(unit)
        if dev is None:
            return None                       # 不存在的位址：不回應（同真實 bus）
        if random.random() < self.drop:
            if self.verbose:
                print("[%s] unit %d 故意不回應" % (tag, unit))
            return None
        resp = handle_pdu(dev, pdu)
        if self.verbose:
            print("[%s] unit %d %s  req=%s  resp=%s" % (tag, unit, dev.name, pdu.hex(" "),
                                                     resp.hex(" ") if resp else "-"))
        return resp


# ---------- Modbus TCP ----------
def serve_tcp(bus, port):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", port))
    srv.listen(4)
    print("Modbus TCP 監聽 0.0.0.0:%d" % port)

    def client(conn):
        with conn:
            buf = b""
            while True:
                data = conn.recv(1024)
                if not data:
                    return
                buf += data
                while len(buf) >= 7:
                    tid, pid, length, unit = struct.unpack(">HHHB", buf[:7])
                    if len(buf) < 6 + length:
                        break
                    pdu = buf[7:6 + length]
                    buf = buf[6 + length:]
                    resp = bus.dispatch(unit, pdu, "TCP")
                    if resp is not None:
                        conn.sendall(struct.pack(">HHHB", tid, pid, len(resp) + 1, unit) + resp)

    while True:
        conn, _ = srv.accept()
        threading.Thread(target=client, args=(conn,), daemon=True).start()


# ---------- Modbus RTU ----------
def serve_rtu(bus, port, baud, parity, stopbits):
    import serial
    ser = serial.Serial(port, baud, parity=parity, stopbits=stopbits, timeout=0)
    char_t = 11.0 / baud
    gap = max(3.5 * char_t, 0.00175)          # 3.5 字元靜默（>19200 固定 1.75ms）
    print("Modbus RTU 開在 %s %d 8%s%d（frame gap %.2f ms）" % (port, baud, parity, stopbits, gap * 1000))
    buf = b""
    last = time.perf_counter()
    while True:
        data = ser.read(256)
        now = time.perf_counter()
        if data:
            buf += data
            last = now
            continue
        if buf and now - last >= gap:
            frame, buf = buf, b""
            if len(frame) < 4 or crc16(frame[:-2]) != struct.unpack("<H", frame[-2:])[0]:
                if bus.verbose:
                    print("[RTU] 丟棄壞 frame: %s" % frame.hex(" "))
                continue
            unit = frame[0]
            if unit == 0:
                continue                      # 廣播：本模擬不處理
            resp = bus.dispatch(unit, frame[1:-2], "RTU")
            if resp is not None:
                out = bytes([unit]) + resp
                c = crc16(out)
                if random.random() < bus.crc_err:
                    c ^= 0x5A5A
                ser.write(out + struct.pack("<H", c))
        time.sleep(0.0002)


# ====================================================================
# 物理迴圈 / 狀態顯示 / 自我測試
# ====================================================================
def physics_loop(devices, show):
    world = {}
    dt = 0.1
    t_show = 0.0
    while True:
        for d in devices.values():
            with d.lock:
                d.step(dt, world)
        t_show += dt
        if show and t_show >= 2.0:
            t_show = 0.0
            v, tc = devices[1], devices[2]
            print("VFD %5.2f Hz %4.1f A | 爐溫 PV %5.1f°C SV %5.1f 輸出 %5.1f%% %s" % (
                v.regs[v.FREQ_OUT] / 100, v.regs[v.CURRENT] / 10,
                tc.regs[tc.PV] / 10, tc.regs[tc.SV] / 10, tc.regs[tc.OUT] / 10,
                "【高溫警報】" if tc.regs[tc.ALM_ST] else ""))
        time.sleep(dt)


def selftest():
    devs = {1: FakeVFD(), 2: FakeTempController()}
    bus = Bus(devs)
    ok = True

    def check(name, cond):
        nonlocal ok
        cond = bool(cond)
        ok &= cond
        print(("PASS " if cond else "FAIL ") + name)

    check("CRC 標準向量 01 03 00 00 00 01 → 0x0A84",
          crc16(bytes.fromhex("010300000001")) == 0x0A84)
    r = bus.dispatch(2, bytes.fromhex("03 10 00 00 02"), "T")
    check("讀溫控 PV/SV", r == bytes.fromhex("03 04 00 fa 03 20"))
    check("寫唯讀 PV → 例外 02", bus.dispatch(2, bytes.fromhex("06 10 00 01 00"), "T") == bytes.fromhex("86 02"))
    check("讀不存在位址 → 例外 02", bus.dispatch(1, bytes.fromhex("03 30 00 00 01"), "T") == bytes.fromhex("83 02"))
    check("頻率 70Hz → 例外 03", bus.dispatch(1, bytes.fromhex("06 20 01 1b 58"), "T") == bytes.fromhex("86 03"))
    check("FC16 寫頻率 30Hz + 運轉",
          bus.dispatch(1, bytes.fromhex("10 20 00 00 02 04 00 12 0b b8"), "T") == bytes.fromhex("10 20 00 00 02"))
    check("不支援的功能碼 → 例外 01", bus.dispatch(1, bytes.fromhex("2b 0e 01 00"), "T") == bytes.fromhex("ab 01"))
    check("不存在的 unit 不回應", bus.dispatch(9, bytes.fromhex("03 00 00 00 01"), "T") is None)
    world = {}
    for _ in range(60):                       # 6 秒
        devs[1].step(0.1, world)
    check("加速 6 秒後 ≈30Hz 且到達", devs[1].regs[FakeVFD.FREQ_OUT] == 3000 and devs[1].regs[FakeVFD.STATUS] & 0x4)
    devs[2].regs[FakeTempController.RUN] = 1
    for _ in range(600):
        devs[2].step(0.1, {})
    check("溫控運轉 60 秒後升溫", devs[2].regs[FakeTempController.PV] > 400)
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description="模擬 PLC 外設（變頻器 unit1 / 溫控器 unit2）")
    ap.add_argument("--tcp", type=int, metavar="PORT", help="Modbus TCP 埠（建議 5020）")
    ap.add_argument("--rtu", metavar="COM", help="Modbus RTU 序列埠（com0com 虛擬埠）")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--parity", default="E", choices="NEO")
    ap.add_argument("--stopbits", type=int, default=1, choices=(1, 2))
    ap.add_argument("--drop", type=float, default=0.0, help="不回應機率 0~1")
    ap.add_argument("--crc-err", type=float, default=0.0, help="RTU 回應 CRC 錯誤機率 0~1")
    ap.add_argument("-v", "--verbose", action="store_true", help="印出每筆請求/回應")
    ap.add_argument("--quiet", action="store_true", help="不印週期狀態")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()

    if a.selftest:
        sys.exit(selftest())
    if not a.tcp and not a.rtu:
        ap.error("至少指定 --tcp 或 --rtu")

    devices = {1: FakeVFD(), 2: FakeTempController()}
    bus = Bus(devices, a.drop, a.crc_err, a.verbose)
    threading.Thread(target=physics_loop, args=(devices, not a.quiet), daemon=True).start()
    if a.tcp:
        threading.Thread(target=serve_tcp, args=(bus, a.tcp), daemon=True).start()
    if a.rtu:
        threading.Thread(target=serve_rtu, args=(bus, a.rtu, a.baud, a.parity, a.stopbits),
                         daemon=True).start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("結束")


if __name__ == "__main__":
    main()
