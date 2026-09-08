# RP2040 中繼器（模組 B · MicroPython）

由 [../repeater_micropython/](../repeater_micropython/)（Nucleo 版）移植，只換 UART 腳位 + 抽出設定區。行為與 C++ [../nucleo_repeater/](../nucleo_repeater/) 一致。

## 深鏈拓撲 1-2-3-7（首發，7 顆 13487）

```
PC ─busU─ [R1 addr=1] ─busM─ [R2 addr=2] ─busL─ [R3 addr=3] ─busXL─ [T7 addr=7]
```

三台中繼器燒**同一支 main.py**，每片只改 `MY_ADDR`（1/2/3）；`CHILDREN` 三台都是 `[7]`。

| 板 | MY_ADDR | CHILDREN | up(GP4/5) 接 | down(GP0/1) 接 |
|---|---|---|---|---|
| RP2040-Lite (R1) | 1 | [7] | busU → USB-485 dongle | busM → R2.up |
| RP2040-ETH#1 (R2) | 2 | [7] | busM → R1.down | busL → R3.up |
| RP2040-ETH#2 (R3) | 3 | [7] | busL → R2.down | busXL → T7 |

末端 T7：STM32（F411 Pico 或 Nucleo），跑 [../stm32_terminal/](../stm32_terminal/)，`FIXED_ADDR=7`。

## 上傳（每台 RP2040）

1. BOOTSEL 進 RPI-RP2 → 燒一次 MicroPython UF2
2. Thonny 上傳 **`main.py` + `lib/protocol.py`**（同層）到板子
3. 燒前改 `MY_ADDR`
4. REPL 每秒印 `[repeater] up_rx=.. down_rx=.. frames=.. cache[0]=..`

## 驗證（PC 對 dongle 的 COM）

```
python rs485_probe.py COM<dongle> 7
```
應收到 `1-2-3-7|{...,"age":N}`。

## 收發器 / 供電 / 準位（重要）

- 收發器 **MAX13487EESA**（8-SO、**5V**、AutoDirection，免 DE/RE）→ 無方向控制碼。換 13485 見 main.py 檔尾註解。
- 每顆 MAX 由**它所連板子的 `5V`(VBUS)** 供電（**不是 3V3**；13487 最低 VCC ≈4.5V）。GND 全系統共地。
- ⚠️ **RP2040 不是 5V 容忍**（腳位最高 3.6V），而 5V 供電的 13487 `RO` 會擺到 ~5V：
  - **RO → RP2040 RX：每條都要降壓**（分壓電阻，38400 慢速夠用：`RO ─1.8k─ RX ─3.3k─ GND`，5V→~3.24V）。
    每台 RP2040 中繼器 up/down 兩條 RX → **各 2 組分壓**。
  - **RP2040 TX → DI：** 3.3V 通常能驅動（13487 VIH≈2V），保險查你批號 datasheet。
- **STM32 末端**：USART1 RX(PA10) 是 5V 容忍 FT 腳 → `RO` **直接接、免分壓**（見 stm32_terminal.ino 第 12 行）。
- 每段 bus 兩端各 120Ω 終端；A/B/**GND** 三線，全系統共地（USB 全插同一 hub 即共地）。
