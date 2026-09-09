# 08 · 末端外設 / Driver / 系統韌性架構

> 本文彙整定版決策，作為日後實作依據。與既有文件相容：框架/定址見 [02](02_通訊協定.md)、[03](03_定址與指撥開關.md)；設備描述見 [../schema/](../schema)；設計決策見 [06](06_設計決策與待確認.md)。
> 現行實測基準：深鏈 `1-2-3-7`（3×RP2040 中繼 × 1×STM32 末端）已端到端驗通，見 [bringup_1237.md](bringup_1237.md)、[hardware_1237.svg](hardware_1237.svg)。

---

## 1. 目的

把系統從「末端=單一感測器」升級為「**末端=可掛多種異質外設、可本地運算的智慧節點**」，同時保持既有的樹狀定址、前綴聚合、快取解耦不變。

## 2. 名詞

| 名詞 | 定義 |
|---|---|
| **Node（末端節點）** | 一顆 MCU = 一個 bus 位址。掛 0..N 個外設。 |
| **Peripheral（外設）** | 掛在 node 上的一個單元，有 local id（= path 最深段）。分**物理**（感測/驅動）與**邏輯功能塊**（PID/邊緣AI）。 |
| **Driver** | 某 `type` 對應的採集+換算實作。 |
| **Segment（段）** | 兩台之間的一條獨立 RS485 bus（各自收發器/終端/baud）。 |
| **Path（位址路徑）** | 中繼器鏈 + node + 外設，每段 1–63，例 `1-2-3-7-1`。 |

## 3. 硬體基準（角色與平台）

| 角色 | 平台 | 語言 | UART | 備註 |
|---|---|---|---|---|
| 中繼器 | **RP2040** | MicroPython | 雙（up=slave/PC 側、down=master/子側） | up/down 各一顆 MAX |
| 末端 | **STM32**（FT 5V 容忍） | C++ | 單 bus + 本地外設 | 可跑 PID/AI |
| 上位機(D) | PC | Python | — | rs485_monitor / rs485_probe |

### 3.1 末端 MCU 分級（2026-09-09）

| 級 | MCU | 適用 | 為什麼 |
|---|---|---|---|
| **通用（預設）** | **STM32 F411** | 感測精量測、馬達/快控制、一般控制節點 | 硬體 **FPU**(float PID 快) + **16ch ADC** + 進階 timer(互補 PWM/死區/編碼器) + **FT 5V 容忍腳**(5V 收發器免分壓) |
| **特用** | **RP2040（C++）** | IO 古怪、要雙核隔離硬即時迴路、大 RAM 緩衝 | 雙核 + PIO + 264K RAM；⚠️ **無 FPU**(float 慢/要定點)、**ADC 弱**(4ch 雜訊)、**非 5V 容忍**(要分壓) |
| **重運算** | **STM32 F7/H7、ESP32-S3** | 多迴路 + 邊緣 AI + 大 RAM | 算力/RAM/NPU |

> 中繼器用 RP2040/MicroPython（雙 UART 好，見 [06 §J](06_設計決策與待確認.md)）；末端偏 STM32（ADC/FPU/馬達/5V 容忍）。「平台一致(全 RP2040) vs 各取所長」自行權衡：一般/慢控制可 RP2040，感測精量測+馬達快控制用 F411，重運算上 F7/H7。

### 3.2 收發器選型

| 件 | VCC | 方向 | 偏壓 | 分壓(對 RP2040) | 速度 | 定位 |
|---|---|---|---|---|---|---|
| **MAX13487**（現行） | 5V | AutoDir（免管） | **要** | 要 | 500k | 手上現貨、深鏈用 |
| MAX13485E | 5V | 手動 DE/RE | **免**(true fail-safe) | 要 | 16M(86E) | 免偏壓、要控向 |
| **TI 3.3V fail-safe**（THVD2450 / SN65HVD75 / 3082E） | 3.3V | 手動 DE/RE | **免** | **免** | 200k–20M | **最乾淨**：免偏壓+免分壓 |

**分段角色選件**（速度堆上面、距離堆下面）：
- **葉端段**（少節點、可能長）→ 中速 fail-safe，115200，長距 ~1200m。
- **主幹段**（匯聚多節點、通常短）→ 高速 RS485(Mbps)，**頻寬牆在此**。
- **頂層**（大樹）→ 直接 **Ethernet(W5500)**（RP2040-ETH 已內建）→ 頻寬牆外推到近乎無限。

**速度×距離（反比）**：≤90kbps→~1200m、1Mbps→~120m、10Mbps→~12m。工廠傳感器＝低速長距＝RS485 甜蜜點，用**慢邊緣件**。線材要**雙絞**（非麵包板跳線）。

### 3.3 準位 / 方向控制

- **RP2040 非 5V 容忍** → 5V 收發器 `RO→RX` 分壓 1.8k/3.3k(5V→3.24V)；**STM32 FT 腳 或 3.3V 收發器 → 直接**。收發器 `VCC` 取板子 5V(VBUS) 或 3.3V(依件)。
- **手動 DE/RE 件**：DE(高有效)+/RE(低有效) **綁同一 GPIO**（高=發送/低=接收）；⚠️ 發送前拉高、**`uart.txdone()`/`flush()` 後才放**（放早截尾巴）。韌體留 `MANUAL_DIR` 旗標好切回 AutoDir 件。

## 4. 定址模型

```
1 - 2 - 3 - 7 - 1
│   │   │   │   └ 外設 local id（node 內排序）
│   │   │   └ node（末端，一顆 MCU）
└───┴───┴ 中繼器鏈（每經一台前綴一段）
```

- **外設 = 最深一段 path**：一等公民，可被單獨定址 / 路由 / 下規則 / 各自健康狀態。
- **位址 = 位置（where）**；**descriptor = 定義（what）**。位址不告訴你外設是什麼，要靠 `DESC` 抓說明書對映。
- 樹狀 + 前綴聚合：每層把孩子路徑接上自己位址（`7-1` → `3-7-1` → … → `1-2-3-7-1`）。

## 5. 兩種 JSON × 三種請求

| 請求 | 回什麼 | 時機 |
|---|---|---|
| `DESC\|<addr>` | **設備說明書**（身分/能力/外設清單/type/binding/driver/校正） | 開機自報一次 + PC 索引整體架構時重問 |
| `REQ\|<addr>` | **量測 payload**（即時值；多外設用 batch `\n…\r`） | 每輪輪詢 |
| `SET\|<addr>\|<value>` | **寫指令/參數**（驅動輸出、PID setpoint/Kp Ki Kd、AI enable） | PC/規則引擎下行 |

> `SET` 接上 schema 既有的 `role:"output"` / `output.channels` 佔位。

## 6. 設備說明書（descriptor）擴充

現行 [device.schema.json](../schema/device.schema.json) 一份 = 單一 `type`，撐不住「一顆末端多種外設」。擴充為 **node = `peripherals[]` 容器**：

```
node descriptor
├ node 身分（model/vendor/fw/id）
└ peripherals[]
    ├ id            外設 local id（= path 最深段）
    ├ type          TC-K / PRS-420 / LOADCELL / FLOW / PID / AI-xxx …
    ├ binding       採集介面：adc(PA0) / spi(max31855) / i2c / pulse(gpio)
    │               邏輯塊：in=<外設id> / out=<外設id> / model=…
    ├ driver        對應 type 的實作（見 §7）
    ├ calibration   slope/offset…（換算用，見 §7）
    └ measurements  產出值 key/unit/min/max/resolution（沿用現有欄位）
```

每個 peripheral ≈ 一份現有 device.schema 內容，多 `binding`+`driver`。

## 7. 外設與 Driver 模型

### 7.1 兩類外設

| 類 | 例 | binding | REQ 回 | SET 寫 |
|---|---|---|---|---|
| 物理感測 | 熱電偶/壓力/流量/重量 | adc/spi/i2c/pulse | 量測值 | — |
| 物理驅動 | 繼電器/PWM | gpio/pwm | 輸出狀態 | 開關/類比值 |
| 邏輯·PID | 閉環控制 | in=感測外設, out=驅動外設 | sp/pv/out/mode | setpoint、Kp/Ki/Kd |
| 邏輯·邊緣AI | 本地推論 | in=感測外設, model | 推論結果/分類 | enable、切模型 |

### 7.2 type → Driver 登錄表

- `driver = 採集(binding) + 換算(raw→工程值)`。加新感測器 = **加一個 driver + 一筆 descriptor entry**，框架/協定不動。
- **採集介面不限 ADC**：熱電偶常用 MAX31855(SPI) 或 ADC+冷端補償、荷重元用 HX711、流量用脈衝計數(GPIO 中斷)、4–20mA 壓力→ADC。
- **介面預留狀態機 hooks**：`init() / start() / ready() / read() / status()`。簡單感測器只實作 `read()`；狀態機型（DS18B20「啟動轉換→等→讀」、荷重元 tare）**日後補、不改框架**。
- **換算只做一次**：driver 出 **raw**；工程換算讀 descriptor 的 `calibration` 套一次（不在 driver 又套一遍）。
- **unknown type**：標 `unsupported`，不 crash，之後補 driver。
- **錯誤隔離**：每 peripheral 各自 `ok`；壞的標 0、其他照回 → 讓人去換。

### 7.3 邏輯功能塊（PID / AI）

- 是 `peripherals[]` 裡的一員、一樣有位址。
- **本地迴圈以自己速率背景跑**，與輪詢解耦；REQ 只讀其最新狀態槽，SET 改其參數。
- **本地自主**：上層斷線時 PID 仍在本地閉環控制製程（見 §9）。

### 7.4 節點分類與外設容量（2026-09-09）

**節點型別**（按「功能 × 速度/關鍵度」分，`role` 對應 schema）：

| 節點型 | role | 內容 | 速度 | 聚合/獨立 |
|---|---|---|---|---|
| 監控節點 | `sensor` | 只讀感測（可一二十個） | 慢 | 純輸入 → **聚合省定址** |
| 慢控制節點 | `controller` | 感測+作動+慢 PID（可數個迴路） | Hz | I/O 同在 → 可聚合 |
| **快控制節點** | `controller` | **1–2 個**快迴路（馬達 kHz） | kHz | 硬即時 → **獨立避抖動** |
| 輸出節點 | `output` | PC 規則/上層驅動的作動 | — | 跨節點協調 → 收 `SET` |
| 安全節點 | `safety` | E-stop/互鎖/safe_state | — | 高可靠 → 故障隔離 |

**拆分 5 規則**：① **閉環不跨節點**（PID 的感測+作動+運算必同顆）② **快慢分家**（kHz 迴路獨立）③ **關鍵度分家**（安全/高價值獨立）④ **電源域分家**（大電流作動 vs 敏感類比）⑤ **輸出看它聽誰的**（見下）。

**輸出(作動)三歸屬**：
- 作動**看得到自己迴路的感測** → **控制節點**（跟感測綁一顆，本地閉環）。
- 作動**要靠別處資訊** → **輸出節點**（`role:output`，收 PC 的 `SET`）。
- 作動**要緊急強制安全** → **安全節點**（收廣播 addr 0 → safe_state）。

**外設容量（受 I/O 腳位/迴路速率限，非 PID 算力）**：
- PID 運算很輕（F4 有 FPU，一次更新 µs 級）→ **算力幾乎不是牆**。
- **63 個「慢」外設 + 慢 PID：可以**（F411 + I2C 擴充 = ADS1115/PCA9685/MCP23017 湊通道）→ **PLC 式掃描節點**，全掃 ~10–30Hz；瓶頸是 **I2C 頻寬 + 掃描時間**，不是 CPU。
- **63 個「快」PID(kHz)：不行** → 快迴路不能走慢 I2C 擴充、且 saturate CPU → 快迴路要**少量 + 直接原生快腳 + 獨立節點**。
- 甜蜜點：**少量快（原生 ADC/PWM）+ 大量慢（I2C 擴充）**；一顆 F411 實務 ~8–16 感測 + 4–8 作動，含 1–2 快迴路。
- **63 塞一顆 vs 拆多節點**：塞一顆省定址(1 位址)但單點故障+共用掃描率；拆節點多定址但故障隔離+各自速率。→ 慢的可聚合、**快/高關鍵度分出去**。

## 8. 輪詢與快取（cache-decoupling / hold-last）

- 每輪**固定短 timeout**；沒回就撈**最終快取值**（標 stale），**不等更新**。（幾百節點輪一圈很久，本來就該這樣。）
- **node 一次回全部外設（batch）**；中繼器**收整批、快取多筆**（協定本就支援 batch：decoder 每筆帶 `final` 旗標，`\n`=後面還有、`\r`=最後一筆）。
- **慢 driver / PID / AI 在背景更新自己的快取槽**，輪詢永不等它 → 慢感測器不卡 bus。
- **健康 envelope（跨設備通用）**：`ok`(自檢) / `up`(uptime，歸零=剛重開或換板) / `rst`(重置原因)。位址不放 payload（在框架前綴裡）。

## 9. 系統韌性（省成本方案，本版採用）

中繼器單點故障（某台死→其整個子樹全黑）以**「診斷 + 自愈 + 快換 + 自主」**處理，不加冗餘硬體：

1. **故障定位**：某中繼器死 → 其**子樹同時全 stale**、而兄弟正常 → PC 由 blast radius 推出「該換的是那一台共同祖先」，非底下數十台。靠 fresh/stale/消失 狀態機 + 樹結構。
2. **自愈**：中繼器 IWDG 看門狗，暫時當機自重開。
3. **快換 + 冷備**：中繼器是廉價 RP2040，備品架上放；換上 `up≈0` 系統自知是新板。
4. **本地自主兜底**：有 PID 的節點在斷線期間**製程不失控**，只是遙測暫停；中繼器換好自動接回。

> 冗餘路徑（dual-home/環狀/雙機熱備）非本版範圍。

## 10. 每段 baud / 距離 / 容量

- **每段獨立**：段被中繼器電氣隔開 → **各段可跑不同 baud**。中繼器 = **rate bridge**，`up_baud ≠ down_baud`（現行單一 `BAUD` 常數要放寬）。
- **距離定 baud**（RS485）：葉子段（近、快，如 115200）、主幹段（遠、慢，如 9600，可達 km 級）。
- **慢主幹不拖垮快葉子**：cache 解耦 → 中繼器本地用快葉子更新 cache，主幹只以慢速率搬**聚合後**資料。
- **容量每段各算**，**最慢段（長主幹）= 該路徑瓶頸**；概估 `baud→bytes/s→frames/s→撐幾個 node×外設/輪詢週期`，用 [../sim/capacity.py](../sim/capacity.py)、[../sim/link_budget.py](../sim/link_budget.py) 估。距離另吃終端/偏壓/線材（黃金法則，見 [known_good_baseline.md](known_good_baseline.md)）。

## 11. 沿用不變

框架 `<path>|<payload>\r`（batch 用 `\n`）、`FrameDecoder` per-record、位址操作（prefix/split/valid 1–63）、CRC 保留未啟用、健康 envelope。皆見 [02](02_通訊協定.md) / [../schema/](../schema)。

## 12. 對現有韌體的影響（僅記錄，本文不改碼）

1. **中繼器**：`poll_child` 由「收第一框就返回」→ **收整個 batch、快取多筆**；允許 `up_baud ≠ down_baud`。
2. **末端**：由「回固定一筆」→ **本地版中繼器**（serve 每個外設 + 支援 `DESC`/`REQ`/`SET`）。
3. **新增** driver 登錄表 + 背景取樣快取；新增 `SET` 動詞。
4. **schema**：descriptor 擴 `peripherals[]` + 每項 `binding`/`driver`。

## 13. 已知議題

- **age 疊層**（cosmetic）：多跳回傳 JSON 每經一台中繼各加一次 `age`（如 `"age":5,"age":19,"age":15`）。功能無礙；日後改 `serve_slot` 偵測已有 age 就不重加。
