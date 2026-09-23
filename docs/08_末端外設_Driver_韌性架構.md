# 08 · 末端外設 / Driver / 系統韌性架構

> 本文彙整定版決策，作為日後實作依據。與既有文件相容：框架/定址見 [02](02_通訊協定.md)、[03](03_定址與指撥開關.md)；設備描述見 [../schema/](../schema)；設計決策見 [06](06_設計決策與待確認.md)。
> 現行實測基準：深鏈 `1-2-3-7`（3×RP2040 中繼 × 1×STM32 末端）已端到端驗通，見 [bringup_1237.md](bringup_1237.md)、[hardware_1237.svg](hardware_1237.svg)。

---

## 1. 目的

把系統從「末端=單一感測器」升級為「**末端=可掛多種異質外設、可本地運算的智慧節點**」，同時保持既有的樹狀定址、前綴聚合、快取解耦不變。

## 2. 名詞

| 名詞 | 定義 |
|---|---|
| **Node（末端節點）** | 一顆 MCU = 一個 bus 位址（樹狀定址的最深單位）。掛 0..N 個外設。 |
| **Peripheral（外設）** | node 底下一個單元，有 **local id（descriptor 索引，非 path 段、不進樹狀定址）**。分**物理**（感測/驅動）與**邏輯功能塊**（PID/邊緣AI）。外部外設由 node **MODBUS master** 讀進 cache。 |
| **Driver** | 某 `type` 對應的採集+換算實作。 |
| **Segment（段）** | 兩台之間的一條獨立 RS485 bus（各自收發器/終端/baud）。 |
| **Path（位址路徑）** | 中繼器鏈 + node，每段 1–63，例 `1-2-3-7`（**只到 node，不含外設**）。 |

## 3. 硬體基準（角色與平台）

| 角色 | 平台 | 語言 | UART | 備註 |
|---|---|---|---|---|
| 中繼器 | **RP2040** | MicroPython | 雙（up=私有 slave、down=葉段 **MODBUS master** 對末端） | up/down 各一顆 MAX |
| 末端 | **STM32**（FT 5V 容忍） | C++ | 雙（up=**MODBUS slave** 對中繼器、down=**MODBUS master@115200** 對外部外設） | 可跑 PID/AI；外設值進 cache |
| 上位機(D) | PC | Python | — | rs485_monitor / rs485_probe |

### 3.1 末端 MCU 分級（2026-09-09）

| 級 | MCU | 核心 / 關鍵特性 | 適用 |
|---|---|---|---|
| **通用（預設）** | **STM32 F411** | M4F@100MHz、SP FPU、12-bit ADC 16ch、**FT 5V 容忍腳**(5V 收發器免分壓) | 感測精量測、馬達/快控制、一般控制節點 |
| （過渡）更快 F4 | STM32 F446 | M4F@**180MHz**、3× ADC、更多 timer | 同級 RAM 但要更快/更多外設時 |
| **特用** | **RP2040（C++）** | 雙核 M0+@133MHz + **PIO** + 264K RAM；⚠️ **無 FPU**(float 慢)、**ADC 弱**(4ch 雜訊)、**非 5V 容忍**(要分壓) | IO 古怪、要雙核隔離硬即時迴路、大緩衝 |
| **重控制** | **STM32 F7** | **M7**@216MHz + **L1 cache** + 256–512K RAM（F76x 為 **DP FPU**）、3× 12-bit ADC、M7 DSP 指令 | 更快/更多迴路、更大 RAM、中等邊緣 AI |
| **邊緣AI / 精量測** | **STM32 H7** | M7@**480MHz** + **雙精度 FPU** + L1 cache + **1MB+ RAM** + **16-bit ADC(3.6MSPS)**；部分**雙核**(M7@480 + M4@240) | 邊緣 AI(CMSIS-NN)、**精密感測(16-bit)**、高速多迴路、**雙核隔離**(M7 跑 AI / M4 跑硬即時) |
| **專用 AI** | **STM32 N6** | Cortex-M55 + **Neural-ART NPU**(硬體加速) | 真正的 CNN 推論 |

**何時往上跳**：
- **F411 → F7**：要**更快/更多迴路**、**更大 RAM**、cache 加速；M4→M7 是質變。
- **F7 → H7**：要**邊緣 AI**(DP FPU+大 RAM+CMSIS-NN)、**精密類比**(16-bit ADC vs 12-bit)、或**雙核隔離**(一核 AI、一核硬即時，等於 RP2040 的雙核但有 FPU+算力)。
- **H7 → N6**：要跑**真 CNN**(NPU 硬體加速，非軟體推論)。

> ⚠️ **去陸製**：邊緣 AI **不用 ESP32-S3**(Espressif 中國) → 非陸製 AI 路線是 **STM32 H7 / N6**。
> 中繼器用 RP2040/MicroPython（雙 UART，見 [06 §J](06_設計決策與待確認.md)）；末端偏 STM32（ADC/FPU/馬達/5V 容忍）。「平台一致(全 RP2040) vs 各取所長」自行權衡。

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

## 4. 定址模型（2026-09-16 修正：外設不進樹狀定址）

```
1 - 2 - 3 - 7          ← path 只到 node（末端）
│   │   │   └ node（末端，一顆 MCU）= 樹狀定址最深單位
└───┴───┴ 中繼器鏈（每經一台前綴一段）

     └ node 內：外設由 descriptor 索引（id 0,1,2…），非 path 段
```

- **path 只到 node**（不再是 `1-2-3-7-1`）。**外設不進樹狀定址**——由 node MCU 抓值（外部外設走 MODBUS@115200）放進本地 cache，靠 **descriptor 索引**組織。要某外設值＝向該 **node** `REQ`，由它從 cache 提供（descriptor 定順序）。
- **位址 = 位置（where，到 node）**；**descriptor = 定義（what，含外設清單與抓取方式）**。
- 樹狀 + 前綴聚合：每層把孩子路徑接上自己位址（`7` → `3-7` → … → `1-2-3-7`）。
- **理由**：外設數量多時，若每個都佔一個 bus 位址會爆掉且與外部 MODBUS 設備衝突；改由 node 聚合、cache、以 MODBUS 暫存器對上呈現，定址乾淨、與 MODBUS 相容。（取代先前「外設＝一等公民可單獨定址」模型，見 [06 §N](06_設計決策與待確認.md)。）

### 4.1 傳輸分層（edge=MODBUS / core=私有）

| 鏈路 | 格式 | master | 速度 |
|---|---|---|---|
| 末端 ↔ 外部外設 | **MODBUS RTU** | 末端 | 115200 |
| 末端 ↔ 中繼器（葉上行） | **MODBUS RTU**（末端 cache 值 = 暫存器） | 中繼器 | 依葉段 |
| 中繼器 ↔ 中繼器／PC（骨幹） | **私有格式 `0xFE`** + `*<crc>\r` | 逐跳前綴 | **高速** |

**第一台中繼器**把末端 MODBUS 讀值**轉成私有格式**、開始長 path。第一 byte 判別（`1-63`/`0xFF`=MODBUS、`0xFE`=私有）見 [02 §3.1](02_通訊協定.md)。

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
    ├ id            外設 local id（descriptor 索引 0,1,2…；非 path 段）
    ├ type          TC-K / PRS-420 / LOADCELL / FLOW / PID / AI-xxx …
    ├ binding       本地採集：adc-電壓(PA0) / adc-4~20mA(取樣電阻) / spi(max31855) / i2s / i2c / pulse(gpio)
    │               外部 MODBUS：modbus{regtype/addr/fc/dtype/word_order/scale}
    │               邏輯塊：in=<外設id> / out=<外設id> / model=…
    ├ driver        對應 type 的實作（見 §7）
    ├ calibration   slope/offset…（換算用，見 §7）
    └ measurements  產出值 key/unit/min/max/resolution（沿用現有欄位）
```

每個 peripheral ≈ 一份現有 device.schema 內容，多 `binding`+`driver`。

### 6.1 外部 MODBUS 外設綁定（`binding.modbus`）

末端當 **MODBUS master** 讀外部外設時，descriptor 用一個 `modbus` 區塊定義「怎麼抓、怎麼還原」：

```json
"binding": { "modbus": {
  "slave": 3, "regtype": "input", "addr": 30001, "fc": 4,
  "dtype": "float32", "word_order": "big", "scale": 0.1
}}
```

- `regtype`↔role 對映：離散輸入(0x02)=DI 感測、線圈(0x01)=DO 輸出、輸入暫存器(0x04)=類比感測、保持暫存器(0x03/06)=類比輸出/設定點。Modicon 佈局 3xxxx/4xxxx。
- ⚠️ **`word_order` + `scale` 必填**：MODBUS 字序/縮放錯會「靜默給錯數值、藏好幾個月」——一律在 descriptor 明寫。

### 6.2 末端 → 中繼器的 MODBUS 暫存器佈局（提案，待確認）

末端把 cache 的外設值以 **MODBUS 暫存器**呈現給中繼器輪詢。**佈局由末端自定義**（你之後拍板）；以下為預設提案：

- 外設值依 descriptor 順序排進**輸入暫存器**（`3xxxx`，FC04）：外設0→30001、外設1→30002…（多字型別佔連續 word，`word_order` 隨 descriptor）。
- 健康/狀態（`ok`/`up`/`rst`）另置一段保留暫存器；設定/`SET` 走**保持暫存器**（`4xxxx`，FC06/16）。
- 中繼器一次 FC04 讀整塊 → 照 descriptor 還原 → 轉私有格式上骨幹。
- DESC/JSON（首次+10s 存活）走 **`0xFE` 私有 frame**（MODBUS 暫存器塞不下 JSON）。

> 標為**待確認**：確切起始位址、健康段位置、多字型別對齊由末端定案後補。

## 7. 外設與 Driver 模型

### 7.1 兩類外設

| 類 | 例 | binding | REQ 回 | SET 寫 |
|---|---|---|---|---|
| 物理感測（本地） | 熱電偶/壓力/流量/重量/類比訊號 | adc(電壓/4~20mA)/spi/i2s/i2c/pulse | 量測值 | — |
| 物理驅動（本地） | 繼電器/PWM | gpio/pwm | 輸出狀態 | 開關/類比值 |
| **外部 MODBUS 外設** | PLC 感測/驅動模組 | **modbus**（末端=master@115200，見 §6.1） | 讀值（照 descriptor 還原） | 寫 coil/holding reg |
| 邏輯·PID | 閉環控制 | in=感測外設, out=驅動外設 | sp/pv/out/mode | setpoint、Kp/Ki/Kd |
| 邏輯·邊緣AI | 本地推論 | in=感測外設, model | 推論結果/分類 | enable、切模型 |

### 7.2 type → Driver 登錄表

- `driver = 採集(binding) + 換算(raw→工程值)`。加新感測器 = **加一個 driver + 一筆 descriptor entry**，框架/協定不動。
- **末端內建一批通用 driver 庫（我們自行維護、預存於韌體）**，按「讀取訊號格式」分類，descriptor 只負責挑「用哪支 driver + 參數」：

  | 訊號格式 | binding | 典型件/備註 |
  |---|---|---|
  | 電壓（類比） | `adc-電壓` | 直接 ADC；分壓/緩衝依量程 |
  | 4~20mA 電流環 | `adc-4~20mA` | 取樣電阻→ADC，線性換算；**<4mA=斷線偵測** |
  | SPI | `spi` | 熱電偶 MAX31855、外部 ADC/DAC |
  | I2S | `i2s` | 數位麥克風/音訊/振動感測 |
  | I2C | `i2c` | 擴充 ADS1115/PCA9685/MCP23017 湊通道 |
  | 脈衝/計數 | `pulse` | 流量計、編碼器（GPIO 中斷）|
  | 荷重元 | 專用 | HX711（自帶時序）|
  | 外部 MODBUS | `modbus` | 末端=master@115200，見 §6.1 |

  > **後段接什麼，由我們在 descriptor 指定，不靠 bus 自動探索**——對照 [02](02_通訊協定.md)「下層探索」：bus 端（子中繼器/末端）自動探索，**末端↔後段外設端則是預存 driver + 人工 descriptor**。
  > **4~20mA 與電壓同落 ADC 腳，但屬不同 driver**（電流環要取樣電阻、線性換算與斷線偵測），descriptor 明確二選一。
- **介面預留狀態機 hooks**：`init() / start() / ready() / read() / status()`。簡單感測器只實作 `read()`；狀態機型（DS18B20「啟動轉換→等→讀」、荷重元 tare）**日後補、不改框架**。
- **換算只做一次**：driver 出 **raw**；工程換算讀 descriptor 的 `calibration` 套一次（不在 driver 又套一遍）。
- **unknown type**：標 `unsupported`，不 crash，之後補 driver。
- **錯誤隔離**：每 peripheral 各自 `ok`；壞的標 0、其他照回 → 讓人去換。

### 7.3 邏輯功能塊（PID / AI）

- 是 `peripherals[]` 裡的一員、一樣有位址。
- **本地迴圈以自己速率背景跑**，與輪詢解耦；REQ 只讀其最新狀態槽，SET 改其參數。
- **本地自主**：上層斷線時 PID 仍在本地閉環控制製程（見 §9）。

**放置定則（2026-09-16）：控制迴路住在「能同時看到它所有輸入、又搆得到輸出」的最低節點。**

| 迴路輸入範圍 | 住哪 | 速度 |
|---|---|---|
| 單一末端 | 該**末端** | µs（無 bus）|
| 同一子樹跨末端 | 該子樹的**中繼器** | 輪詢週期級（慢）|
| 跨整棵樹 | **根中繼器 / PC** | 最慢 |

- **跨末端 → 上移到最低共同祖先（某台中繼器）**：因為沒有單一末端看得到全部輸入。
- **中繼器選配「控制帽」**：預設仍是啞管（見中繼器儲存分工）；**只有被指派跨節點迴路的那台**才多掛一個控制功能塊。它**本來就快取子樹的值** → 輸入直接讀 cache、輸出用 `SET` 往下發，天生適合。
- **仍可熱換**：控制設定由 **PC 目錄下推**(restorable)、控制狀態**揮發**(換板重收斂)、輸出配 **safe_state**（見 §9）。
- ⚠️ **跨節點 = 慢迴路**：經輪詢週期 + `SET` 下發延遲，只能做慢迴路；**快 + 跨節點的協調控制 polled RS485 做不到**（EtherCAT 領域）→ 會快速互動的元件**硬體上就擺同一末端**（呼應 §7.4 規則① 對「快」閉環）。

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
5. **跨節點控制的安全兜底**：託管在中繼器的**跨節點**迴路（§7.3），該中繼器一斷 → 迴路停 → 輸出作動末端偵測到父節點靜默 → 回 **safe_state**（schema 既有欄位）；換板後 PC **re-push 控制設定 + 狀態重收斂**。→ 跨節點控制失效是「安全停」，不是失控。

> 冗餘路徑（dual-home/環狀/雙機熱備）非本版範圍。

## 10. 每段 baud / 距離 / 容量

- **每段獨立**：段被中繼器電氣隔開 → **各段可跑不同 baud**。中繼器 = **rate bridge**，`up_baud ≠ down_baud`（現行單一 `BAUD` 常數要放寬）。
- **距離定 baud**（RS485）：葉子段（近、快，如 115200）、主幹段（遠、慢，如 9600，可達 km 級）。
- **慢主幹不拖垮快葉子**：cache 解耦 → 中繼器本地用快葉子更新 cache，主幹只以慢速率搬**聚合後**資料。
- **容量每段各算**，**最慢段（長主幹）= 該路徑瓶頸**；概估 `baud→bytes/s→frames/s→撐幾個 node×外設/輪詢週期`，用 [../sim/capacity.py](../sim/capacity.py)、[../sim/link_budget.py](../sim/link_budget.py) 估。距離另吃終端/偏壓/線材（黃金法則，見 [known_good_baseline.md](known_good_baseline.md)）。

## 11. 沿用不變

私有格式 `0xFE <path>|<payload>*<crc>\r`（骨幹；batch 用 `\n`）、`FrameDecoder` per-record、位址操作（prefix/split/valid 1–63，path 到 node）、**CRC-16/MODBUS 啟用（`\r` 前驗）**、健康 envelope。edge 走 MODBUS（見 §4.1、[02 §3.1](02_通訊協定.md)）。皆見 [02](02_通訊協定.md) / [../schema/](../schema)。

## 12. 對現有韌體的影響（僅記錄，本文不改碼）

1. **中繼器**：down 對末端改當 **MODBUS master**（FC03/04 讀末端暫存器塊）；up 對骨幹送 **私有格式**；`up_baud ≠ down_baud`。
2. **末端**：新增 **雙 MODBUS 角色**——up 當 **MODBUS slave**（把 cache 外設值/健康以暫存器呈現，§6.2）、down 當 **MODBUS master@115200** 讀外部外設（§6.1）；DESC/JSON 走 `0xFE` 私有 frame。
3. **新增** driver 登錄表 + 背景取樣快取 + MODBUS master/slave 堆疊；`SET` 走保持暫存器/coil。
4. **schema**：descriptor 擴 `peripherals[]` + 每項 `binding`（含 `modbus` 區塊）/`driver`。
5. **協定庫**：啟用 CRC-16/MODBUS；第一 byte 判別（MODBUS vs `0xFE` 私有）。

## 13. 已知議題

- **age 疊層**（cosmetic）：多跳回傳 JSON 每經一台中繼各加一次 `age`（如 `"age":5,"age":19,"age":15`）。功能無礙；日後改 `serve_slot` 偵測已有 age 就不重加。
