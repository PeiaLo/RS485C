# Stage 1 · STM32 一中繼器 + 末端（半雙工 RS485）

產品平台版：中繼器 = MicroPython（Nucleo-F401RE）、末端 = C++（F103/F401）。都用共用 lib。

| 角色 | 韌體 | 平台 | 語言 |
|------|------|------|------|
| 中繼器 | [repeater_micropython/main.py](repeater_micropython/main.py) | Nucleo-F401RE | MicroPython |
| 末端 | [stm32_terminal/stm32_terminal.ino](stm32_terminal/stm32_terminal.ino) | F103 / F401 | C++ (STM32duino) |

## 前置：工具鏈
- **中繼器**：燒 MicroPython 韌體到 Nucleo-F401RE（micropython.org/download → STM32 → NUCLEO_F401RE），用 Thonny/rshell/mpremote 把 `main.py` + `lib/protocol.py` 丟上板。
- **末端**：Arduino IDE 裝 **STM32 core（STM32duino）**，選對板子；先跑 `sh firmware/sync_lib.sh` 把 `protocol.h/.cpp` 複製進 sketch 資料夾。

## STM32 黑膠丸（DevEBox mini_F4x1）燒錄踩雷
- **沒有 BOOT0 按鈕！** 板上 `K0` 是使用者鍵，不是 BOOT0。**BOOT0 = micro-USB 旁那排的 `BT0` 腳**。
- **進 DFU**：把 `BT0` 短接到旁邊的 `3V3` → 按 `RST`（或重插 USB）→ 電腦出現「STM32 BOOTLOADER」（帶黃驚嘆號就用 **Zadig** 裝 WinUSB）→ Arduino 上傳(DFU) → **拿掉短接、按 RST** 跑程式。
- **序列埠不會自動出現**：黑膠丸沒有 USB-UART 晶片。要 Tools → **USB support = CDC (generic 'Serial' supersede U(S)ART)**，燒進去、跑起來後才會列舉成 COM。板號選 **BlackPill F401CC**。
- **F401CC 是 48 腳**：沒有 PC0~PC12（只有 PC13~15），GPIO 用 PA/PB。
- **ST-Link 備援**：同一排 `BT0 3V3 GND DIO CLK` 的 `DIO=SWDIO、CLK=SWCLK` → Nucleo 的 ST-Link 接這裡（拔 Nucleo CN2 跳線），Upload method 改「STM32CubeProgrammer (SWD)」。

## 接線（半雙工，每台一顆 MAX13487）
```
  [Nucleo-F401RE 中繼器]                    [F103/F401 末端 addr 4]
   USART1 TX(PA9) → DI ┐                    TX1 → DI ┐
   USART1 RX(PA10)← RO ┤MAX13487  ═A/B bus═ RX1 ← RO ┤MAX13487
                       └ A/B ────────────────────────┘ A/B
   上層對 PC：USART2 = ST-Link VCP（插 USB 就是對 PC，免收發器）
   bus 兩端各 120Ω；一處 failsafe 偏壓（上拉A、下拉B）
   共地（各 MAX13487 的 GND 與 MCU GND 相連）
```
- MAX13487 = AutoDirection，**免 DE/RE**。
- MAX13487 供電 5V；RO≈5V → 進 STM32 RX：**該腳是 5V 容忍(FT) 就直接接**，否則 1k/2k 分壓。
- baud 全 bus 一致（預設 115200；`main.py` 與 `.ino` 的 BAUD 要一樣）。

## 跑法
1. 末端燒好 → 它的 USB 監控應印 `[terminal] up, addr=4 ...`。
2. 中繼器上 `main.py`+`protocol.py`，reset → 它會開始每輪 `REQ|4`/`REQ|5` 輪詢下層並存 cache。
3. 從 PC 對中繼器的 VCP（USART2）送 `REQ|4\r` 或 `REQ|ALL\r`，應收到：
   `1-4|{"t":"TMP","v":25.3,"u":"C","ok":1,"age":37}\r`
   （`1-` 是中繼器前綴的路徑；`age` 是這筆離上次更新幾毫秒＝新鮮度。）
4. 拔掉末端 → 該位址**保留最後一筆舊值**繼續回、但 `age` 一直變大（hold-last-value）；接回 → age 歸小。

## 過關條件
- [ ] PC `REQ|ALL` 一次拿到多台末端的 cache（各自 `\r`）。
- [ ] 逐跳前綴正確（末端報 4 → 中繼器回 `1-4`）。
- [ ] 拔線 → 舊值續回 + age 變大；不卡死。

## 下一步（→ Stage 2 兩層）
第 2 台 Nucleo 當第 2 層中繼器：它的 `up`(USART2) 改接 MAX13487 對第 1 層 bus（不再用 VCP），`MY_ADDR` 設它在第 1 層的本地位址，`CHILDREN` 設它底下的末端。路徑就會拼成 `1-32-4`。
