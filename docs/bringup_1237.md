# 深鏈 1-2-3-7 接線通則（架構驗證基準）

**已驗通的參考拓撲**：4 層樹狀，3×RP2040 MicroPython 中繼 × 1×STM32 C++ 末端，端到端聚合 `7 → 3-7 → 2-3-7 → 1-2-3-7` 成立。這是「多層樹狀 + 逐跳前綴 + cache 解耦」架構的實機證明。

```
PC ─USB─ [USB-485] ═busU═ [R1 addr1] ═busM═ [R2 addr2] ═busL═ [R3 addr3] ═busXL═ [T7 addr7]
                          RP2040        RP2040          RP2040           STM32 F411
                          純輸送         純輸送           純輸送            末端
```

- 每台**節點位址**由指撥 / `MY_ADDR` 定；末端**外設段**（如 `1-2-3-7-1`）人為定義。
- 中繼器對上為 slave、對下為 master，逐跳把自己位址**前綴**上去 → 頂層聚合成完整路徑。

## 接線通則

- **串接口訣**：一台的 **down 接下一台的 up**（`down.A↔up.A`、`down.B↔up.B`、`GND↔GND`）。
- **每段 bus** 各自套[電氣黃金法則](known_good_baseline.md)：兩端 2×120Ω + 一組偏壓（fail-safe 件免偏壓）。
- **收發器接法**：`VCC→5V(VBUS)`、`GND→共地`、`/RE→GND`、`/SHDN→5V`。
- **準位**：RP2040 `RO→RX 經分壓 1.8k/3.3k`；STM32 `RO→RX 直接`（FT 腳）；`TX→DI` 一律直接。

## 節點 UART 慣例

| 平台 | up（對上層） | down（對下層） | 備註 |
|---|---|---|---|
| **RP2040 中繼器** | UART1（GP4→DI, GP5←RO 分壓） | UART0（GP0→DI, GP1←RO 分壓） | 雙 UART 是中繼器選 RP2040 的主因；避開 W5500 GP16~21 |
| **STM32 末端** | USART1（PA9→DI, PA10←RO 直接） | — | bus 測試需插 USB-C(VBUS 5V) 才餵得動收發器 |

> 對照圖見 [hardware_1237.svg](hardware_1237.svg)。中繼器/末端選型與角色見 [08 §3](08_末端外設_Driver_韌性架構.md)。
