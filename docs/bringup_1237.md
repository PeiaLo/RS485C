# 開燒速查卡 — 深鏈 1-2-3-7

```
PC ─USB─ [USB-485] ═busU═ [R1 COM14] ═busM═ [R2 COM19] ═busL═ [R3 COM18] ═busXL═ [T7 COM17]
                          YD-Lite       ETH#1          ETH#2           F4 Pico
                          addr1         addr2          addr3           addr7
```

> ✅ **2026-09-08 驗通**：R1 `cache=1-2-3-7 fresh`，全鏈 `7→3-7→2-3-7→1-2-3-7` 成立。
> R2/R3 重燒 MicroPython **v1.29.0**（R1 為 v1.19.1，皆可用）；因 USB 重列舉，R2/R3/T7 的 COM 已變動（見下表）。
> **PC 端也驗通**：dongle=`COM10`，`python rs485_probe.py COM10 7 38400` → `1-2-3-7|{...}` ✅
> ⚠️ 已知小瑕疵：回傳 JSON 的 `age` 疊了 3 層（各中繼各加一次，如 `"age":5,"age":19,"age":15`）；功能無礙，日後可改 `serve_slot` 偵測已有 age 就不重加。

## COM 對照（一眼查）

| COM | 板子 | 角色 | addr | 平台 | 看什麼 |
|---|---|---|---|---|---|
| **COM10** | USB-485 dongle | PC 上位機(D) | — | — | `rs485_probe.py COM10 7 38400` → `1-2-3-7\|{...}` |
| **COM14** | YD-RP2040-Lite | R1 中繼器 | 1 | MicroPython | Thonny REPL：`[repeater] up_rx/down_rx/frames` |
| **COM19** | RP2040-ETH#1 | R2 中繼器 | 2 | MicroPython | Thonny REPL：同上 |
| **COM18** | RP2040-ETH#2 | R3 中繼器 | 3 | MicroPython | Thonny REPL：同上 |
| **COM17** | F4 Pico (F411CE) | T7 末端 | 7 | C++/STM32duino | Serial Monitor：`[terminal] alive addr=7` |

> dongle 的 COM 填你實際那個（跑 rs485_probe 用的）。

## 接線通則

- **RP2040 收發器（M1~M6）**：`RO→RX 經分壓 1.8k/3.3k(5V→3.24V)`；`TX→DI 直接`
- **STM32 收發器（M7）**：`RO→RX 直接（FT 容忍）`；`TX→DI 直接`
- 每顆 MAX：`VCC→板子5V(VBUS)`、`GND→共地`、`RE̅→GND`、`SHDN̅→5V`
- 串接口訣：**一台的 down 接下一台的 up**
- 每段 bus：`A↔A`、`B↔B`、`GND↔GND`；兩端各 120Ω；偏壓 560Ω 整段一組(可選)

---

## R1 · COM14 · YD-RP2040-Lite · addr 1

| MAX | UART | MCU腳 | → MAX | 接到 bus |
|---|---|---|---|---|
| M1 (up) | UART1 | GP4→DI, GP5←RO(分壓) | | **busU** ↔ dongle |
| M2 (down) | UART0 | GP0→DI, GP1←RO(分壓) | | **busM** ↔ R2.up |

供電 USB(COM14)+VBUS 5V。燒：Thonny 傳 `main.py`(MY_ADDR=1)+`protocol.py`

## R2 · COM19 · RP2040-ETH#1 · addr 2

| MAX | UART | MCU腳 | 接到 bus |
|---|---|---|---|
| M3 (up) | UART1 | GP4→DI, GP5←RO(分壓) | **busM** ↔ R1.down |
| M4 (down) | UART0 | GP0→DI, GP1←RO(分壓) | **busL** ↔ R3.up |

避開 W5500(GP16~21)。燒：`main.py`(MY_ADDR=2)+`protocol.py`

## R3 · COM18 · RP2040-ETH#2 · addr 3

| MAX | UART | MCU腳 | 接到 bus |
|---|---|---|---|
| M5 (up) | UART1 | GP4→DI, GP5←RO(分壓) | **busL** ↔ R2.down |
| M6 (down) | UART0 | GP0→DI, GP1←RO(分壓) | **busXL** ↔ T7 |

燒：`main.py`(MY_ADDR=3)+`protocol.py`

## T7 · COM17 · F4 Pico (STM32F411CE) · addr 7

| MAX | UART | MCU腳 | 接到 bus |
|---|---|---|---|
| M7 | USART1 | PA9→DI, PA10←RO(**直接**) | **busXL** ↔ R3.down |

- 供電：bus 測試時要插 **USB-C(VBUS 5V)** 才餵得動 M7；只 ST-LINK(3.3V) M7 沒電
- 燒：ST-LINK SWD（`SWDIO/SWCLK/3V3/GND`），`FIXED_ADDR=7`

---

## 驗證順序（由下往上）

1. **COM17** T7：`alive addr=7`＝末端活
2. **COM18** R3：`down_rx`↑、`cache[0]=3-7 fresh`
3. **COM19** R2：前綴成 `2-3-7`
4. **COM14** R1：聚合成 `1-2-3-7`
5. **dongle COM10**：`python rs485_probe.py COM10 7 38400` → `1-2-3-7|{...}` ✅
