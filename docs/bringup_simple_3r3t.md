# 通訊測試 · 簡化版 3 中繼器 + 3 末端（3R3T）

> 目的：在錨點 [4rep+PicoMini](bringup_4rep_picomini.md) 之前，先用**最少板子**驗到「分支 + 同 bus 多中繼器混掛 + 變長 path」。
> 沿用 proven 3×RP2040（[bringup_1237](bringup_1237.md)）＋ 3× F411 末端。R4/F401/F103 本測不用。

## 1. 拓樸

**R1 下層一條 bus 並掛 R2、R3、T1**（兩個子中繼器 + 一個末端）；R2、R3 各自再帶一個末端。

```
USB ─bus1─ R1 ─bus2─┬─ R2 ─bus3─ T2
                    ├─ R3 ─bus4─ T3
                    └─ T1
```

| bus | 成員 | 說明 |
|---|---|---|
| bus1 | USB-485 dongle ↔ R1.up | PC 骨幹 |
| **bus2** | R1.down ↔ **R2.up + R3.up + T1** | multidrop：兩子中繼器 + 一末端同線 |
| bus3 | R2.down ↔ T2 | 點對點 |
| bus4 | R3.down ↔ T3 | 點對點 |

R1 是 bus2 的 master（輪詢 R2/R3/T1）；R2、R3 對上是 slave、對自己下層是 master。

## 2. ⚠️ 位址模型：現行韌體是「扁平葉位址」

`main.py` 的 cache **以「被輪詢的位址」為 key，每跳只把路徑前綴加長、位址不變**（見 [poll_child/serve_slot](../firmware/rp2040_repeater/main.py)）。因此本測遵守兩條：

1. **CHILDREN = 子樹裡所有葉子的位址**（不是直屬孩子）；每個祖先都要列出底下每顆末端。
2. **葉位址全樹唯一**（同 bus 兩顆同位址會一起回話撞包）。

> 與 docs/03 目標「每 bus 本地位址可重用」不同——那是待實作。proven `1-2-3-7` 因單葉 `7` 剛好沒踩到。

## 3. 逐台設定（葉位址 T1=4 / T2=5 / T3=6）

| 節點 | 板 | 平台 | MY_ADDR / FIXED_ADDR | CHILDREN | 預期 path |
|---|---|---|---|---|---|
| **R1** | RP2040-Lite | MicroPython | `MY_ADDR=1` | `[4,5,6]` | `1` |
| **R2** | RP2040 | MicroPython | `MY_ADDR=2` | `[5]` | `1-2` |
| **R3** | RP2040 | MicroPython | `MY_ADDR=3` | `[6]` | `1-3` |
| **T1** | F411 | C++ | `FIXED_ADDR=4` | — | `1-4` |
| **T2** | F411 | C++ | `FIXED_ADDR=5` | — | `1-2-5` |
| **T3** | F411 | C++ | `FIXED_ADDR=6` | — | `1-3-6` |

- **R1 列全葉 `[4,5,6]`**（子樹含全部）；**R2 只 `[5]`、R3 只 `[6]`**（各自底下唯一末端）。
- **BAUD 全部 38400**；3 顆末端都是 F411 → Arduino 板型相同（BlackPill F411CE），只改 `FIXED_ADDR` 分別編燒。

**輪詢對照**（確認不撞包）：
- R1 在 bus2 發 `REQ|4`→T1 直接答；`REQ|5`→R2 轉發 `1-2-5`（T1/R3 不理）；`REQ|6`→R3 轉發 `1-3-6`。
- R2 在 bus3 發 `REQ|5`→T2；R3 在 bus4 發 `REQ|6`→T3。

## 4. 收發器 / 偏壓（簡述）

- **R1/R2/R3**：沿用 proven 13487（AutoDir）；每台 up+down 各 1 顆。
- **T1/T2/T3**：接 13487→`MANUAL_DIR=0`；接 13485E→`MANUAL_DIR=1`（STM32 DE=PB0）。
- 每條 bus 兩端 120Ω；因各 bus 都有 13487(AutoDir) → **都要一組偏壓 2×560Ω**（放下行 master 側：bus2@R1、bus3@R2、bus4@R3）；bus1 由 dongle 自帶。
- 收發器數量：R1/R2/R3 各 2 + T1/T2/T3 各 1 = **9 顆**。bus2 最擠（4 顆同線）。

## 5. 燒錄要點

- **R1/R2/R3**：proven 板可沿用，但 **CHILDREN 要改成上表值、重傳 `main.py`**（MicroPython，不用重燒 UF2）。連同 `lib/protocol.py` 同層上傳。
- **T1/T2/T3**：`stm32_terminal.ino` 各改 `FIXED_ADDR` (4/5/6) 編譯 → DFU 燒錄（`BT0`短`3V3`+`RST`）。

## 6. 驗證結果（2026-09-23 實測 @115200）— 全 PASS ✅

- ✅ 末端 USB CDC：`[terminal] up, addr=4/5/6`，且 `rst=por`（reset 判定順序修正生效，不再誤報 bor）。
- ✅ 中繼器 REPL：`addr=1/2/3, children=[4,5,6]/[5]/[6]`。
- ✅ `REQ|4/5/6` → `1-4` / `1-2-5` / `1-3-6`，逐跳前綴路徑正確。
- ✅ **變長 path 混存**：深度 2（`1-4`）+ 深度 3（`1-2-5`、`1-3-6`）同系統。
- ✅ **bus2 多中繼器混掛**：R1 同線輪詢 R2/R3/T1 三者互不干擾（葉位址唯一）。
- ✅ **13485E 末端手動 DE/RE（PB0）** 在 bus 上正常回話（MANUAL_DIR 路徑驗通）。
- ✅ age 疊層：深度 2 一個 age、深度 3 兩個 age（已知 cosmetic，見 [08 §13](08_末端外設_Driver_韌性架構.md)）。

### 6.1 Soak 穩定度
- @115200、**366 輪 / ~1098 REQ、0 異常**（全 5V 供電基準）。

### 6.2 斷電恢復（依序 R1→R2→R3→T1→T2→T3 拔插）

| 拔誰 | 監看事件 | blast radius | 恢復 |
|---|---|---|---|
| **R1**（根） | 4/5/6 全 **DOWN** → **BACK** | 全樹（PC 唯一路徑）| ✅ 末端 `up` 不歸零（只 R1 重開）|
| **R2 / R3**（中繼器）| 只對應子樹 **STALE→FRESH** | 侷限子樹（`1-2-5` / `1-3-6`）| ✅ 兄弟不受影響、末端 `up` 不歸零 |
| **T1 / T2 / T3**（末端）| 對應 addr **STALE→RESET(up=5,por)→FRESH** | 侷限 | ✅ 末端真的重開 |

→ 驗到三大韌性：**blast-radius 侷限正確**、**DOWN vs STALE(hold-last-value) 分明**、**恢復乾淨且 up/rst 反映真實重開**。
小插曲（非故障）：**熱插拔共用 bus2 上的節點**會讓兄弟瞬間 STALE 又立刻回（A/B 接觸彈跳/收發器上電瞬態）。

### 6.3 3.3V 收發器特性摸底
把 R1/R2/R3(13487) 與末端(13485E) 硬降 **3.3V 供電 + 免分壓** 的四組對照、以及「**混壓才是掉封元凶**」結論，記在 [known_good_baseline](known_good_baseline.md)。摘要：**短桌面 bus 可跑（全一致 3.3V 達 100%）**，但**超規、弱驅動、長線仍需 3.3V 原生件**。

## 7. 通過後 → 錨點
本測通過即證明「分支 + 同 bus 多中繼器混掛 + 變長 path」；接著回 [4rep+PicoMini 錨點](bringup_4rep_picomini.md) 加 R4(Pico)、深度、13485 手動方向與第一 byte 混流。
