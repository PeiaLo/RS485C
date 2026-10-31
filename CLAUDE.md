# RS485_C / EZ_IO — 專案交接入口

> 給接手的 Claude 冷啟動用。先讀本檔，再按「閱讀順序」讀 docs。
> 本檔只做導覽＋現況＋TODO，細節一律以 docs 為準。更新：2026-11-01。

## 這是什麼

工廠用**多層樹狀 RS485 感測/作動網路**（取代 PLC 做資料蒐集 + PC 端條件控制）。產品名 **EZ_IO**：訴求「好用、通用、易擴充」，擴充由我方提供（產品 + 擴充服務）。

**核心設計哲學（CPO 式）：canonical 核心 + 邊界 adapter**
```
[PC] ──頭段 adapter── [RS485 私有核心樹] ──尾段 adapter── [外部外設]
      可插拔傳輸          不變(可靠/抗噪/長距)   可插拔協定
      有線RS485/乙太       核心文字格式(JSON)     MODBUS 只是其一
      無線WiFi(非陸製)     自動終端+自動定址
```
- **核心（整條樹：末端→中繼器→PC）永遠跑「我們的核心文字格式」（定案 JSON）**；中繼器 **0 轉譯**（只前綴 path + 可選 CRC、不解析 payload）。
- **頭/尾是邊界 adapter**：外界要什麼都在邊界轉；MODBUS 只出現在「末端↔外部外設」尾段內部，對外互通在 PC 頭段。
- 換核心格式（JSON↔KV）只動「末端吐法 + PC 解析」，**中繼器一行不改**。

## 閱讀順序

1. `README.md`
2. **`docs/09_軟體架構定案.md`** — 軟體全貌（單一參考）
3. **`docs/06_設計決策與待確認.md`** §N（編碼）、**§O（彈性定架 v2）** — 決策錨點
4. **`docs/02_通訊協定.md`** §3.1 / **§3.2（canonical 核心 + 邊界 adapter，最新定案）**
5. `docs/08_末端外設_Driver_韌性架構.md` — 末端/driver/韌性/MODBUS 綁定
6. `docs/10_自動終端與拓樸.md` — 三埠、自動終端、**§8 自動定址(ENUM 枚舉)**
7. `docs/03_定址與指撥開關.md` — 定址（已改自動定址，DIP 拿掉）
8. `docs/01_系統架構.md` — 系統架構（三段式）
9. `docs/14_易用性與故障處理.md` + `docs/ez_io_design_notes.md` — EZ_IO 產品/易用/資安
10. `子專案/00_拆分總覽.md` + `A`–`F` — 各模組 kickoff brief
11. 硬體：`docs/07_採購清單.md`（含 v2 硬體增減）、`docs/known_good_baseline.md`、`docs/bringup_*.md`

## 關鍵定案（速記）

- **三段式**：頭段(可插拔傳輸) / 核心 RS485 文字格式(不動) / 尾段(可插拔協定)。
- **核心格式＝JSON**（§3.2；提速後路 KV，換了不動中繼器）。
- **定址＝自動定址（ENUM 鏈枚舉）**，DIP 全拿掉；**編號/身分一律以 JSON 為主**（identity，不依物理位置）；ENUM 退為探索+自動終端。
- **MODBUS 只在末端↔外部外設**（尾段 adapter，末端=master@115200），**不是唯一尾段協定**。
- **控制**：PC＝監督規則；末端＝快迴路/本地閉環（斷線不失控）。
- **CRC＝可選**（初期不設、`CRC_ENABLED=False`、`*<crc>` 槽與 `crc16()` 預留；後可加、**不影響硬體**；`feat/modbus` 已有實作可併）。
- **資安**：挑戰-回應認證 dongle（ATTINY402，密鑰表 + keyed CRC32），見 `docs/Dungle_ATTINY402.ino`。
- **韌性（不冗餘）**：預防→偵測→優雅降級→快速復原；中繼器熱備/冷備、本地 PID 兜底、safe_state。

## 韌體結構（怎麼寫）

| 層 | 寫什麼 | 變動性 |
|---|---|---|
| **共用協定庫**（`lib/`，C++ + Python） | framing(`\n`/`\r`) + path 操作 + 可選 CRC；payload 當**不透明字串** | 寫一次 |
| **中繼器**（`firmware/rp2040_repeater/`，RP2040/MicroPython） | **格式無關純管道**：pass-through + 前綴 path + cache + 自動定址枚舉 master。無感測器/格式邏輯 | 寫一次、通用 |
| **末端＝尾段 adapter**（`firmware/stm32_terminal/`，STM32/C++） | driver 登錄表讀外設（含 MODBUS master 讀外部外設）→ 組 **canonical JSON** → 上吐 | 加 driver，不動中繼器 |
| **PC＝頭段 adapter**（`pc/`、`rs485_monitor.py`/`rs485_probe.py`，Python） | 解析 canonical JSON → 轉消費端格式（SCADA/HMI/MODBUS-TCP）+ 儀表板 + 規則引擎 | 加輸出格式 |

## 現況

- ✅ **設計**：v2 全定案（docs 已互相對齊、無衝突）。
- 🟡 **韌體＝原型**：深鏈 `1-2-3-7`（3×RP2040 中繼 × 1×STM32 末端）端到端驗通；核心 JSON 文字、中繼器 0 轉譯管道已跑；末端吐 JSON。
  **待實作**：自動定址(ENUM)、末端 driver 登錄表 + 多外設 + MODBUS-master 尾段、board 板級庫、CRC(可選)。
- 🟡 **硬體**：雕刻版中繼器 + 末端各一台，**待測**（驗證焊板，照 `docs/known_good_baseline.md` 的乾淨判準 + soak + 斷電恢復）。

## TODO（分工）

- **韌體實作（吃 token、不需硬體寫 code）**：按上表「韌體結構」做 v2——優先 ① 中繼器補自動定址(ENUM) ② 末端 driver 登錄表 + MODBUS-master 尾段 ③ PC 頭段 adapter。
- **硬體測試（要有板子）**：在雕刻版燒錄現有/新韌體，用 `rs485_monitor.py`/`rs485_probe.py` 跑，對照 `known_good_baseline` 判準。
- **文件待處理**：`docs/11 中繼器電路`、`docs/12 EZ_IO 提案` 仍是舊 DIP 版，需改成 v2（自動定址/ENUM/無 DIP）。

## Repo / Git

- remote：`origin = github.com/PeiaLo/RS485C`
- 最新工作在分支 **`docs/bringup-4rep-picomini`**（最完整）；`main` 落後、`feat/modbus` 有 CRC 實作。接手前請先確認分支/合併與 push 狀態。
- 本機 memory（`~/.claude`）**不隨 repo**；repo docs 才是交接真相來源。
