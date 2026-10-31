# 子專案 B · 中繼器韌體（MicroPython / RP2040）

> 平台定案：**RP2040**（雙 UART 是選它的主因；ESP32 去陸製排除、NUCLEO_F401RE MicroPython 僅 1 UART 淘汰，見 [docs/06 §J](../docs/06_設計決策與待確認.md)）。三分類：純輸送 / 高速傳送(W5500) / 計算與驅動(RPi)。

## 目標
寫正式的 MicroPython 中繼器：對上是 slave、對下是 master。

> 傳輸定案（[docs/02 §3.2](../docs/02_通訊協定.md) canonical 核心 + 邊界 adapter）：**中繼器 up/down 全程核心文字 `0xFE`、0 轉譯**——收下層吐的核心文字、**前綴 path +（可選）重算 CRC、不解析/不轉譯 payload**。MODBUS 不在中繼器（只在末端↔外設）。見 [06 §N/§O](../docs/06_設計決策與待確認.md)。

## 範圍
- **要做（純管道）**：round-robin 輪詢下層、收**核心文字** payload、**前綴本地位址 + 存 cache**；dict+新鮮度、逾時跳過、上層優先回 cache；payload **不解析純轉發**（CRC 可選逐跳重算、age/健康 envelope 放框架外，見 [docs/09](../docs/09_軟體架構定案.md)）。
- **格式無關**：中繼器不碰 payload 內容 → 核心格式換 JSON↔KV **一行不改**。
- **要做（自動定址 master）**：對下層當**枚舉 master**——廣播 `enumerate`、依 `ENUM` 鏈序逐台配本地位址（1,2,3…）、換板重枚舉復位；本地 path = 父層 path + 子鏈序（見 [10 §8](../docs/10_自動終端與拓樸.md)、[03](../docs/03_定址與指撥開關.md)）。DIP 已拿掉。
- **要做**：載入型錄(catalog)供 hot-swap（位址自動 + 型錄 descriptor 給身分 → 換同款即自動配置）；`up_baud ≠ down_baud`（葉段近/慢、骨幹快）。
- **先不做**：Ethernet 傳輸（那是 F）、輸出 `SET` 下行（第二版）。

## 依賴契約
- 模組 [C 協定庫](C_協定庫.md)（Python 版）、模組 [E schema](E_schema.md)、[docs/02](../docs/02_通訊協定.md)、[docs/03](../docs/03_定址與指撥開關.md)。

## 交付物
- `firmware/rp2040_repeater/`：`main.py`（`MY_ADDR`、雙 UART、輪詢、cache、前綴）、`board.py`（板級 UART/準位 config）、引用 `lib/protocol.py`。

## 完成定義（DoD）
- 一台中繼器輪詢多台末端，cache 正確；拔掉一台→該格 stale、其餘照跑。
- 末端資料經中繼器後被前綴成 `<本地>-<末端>`（多層聚合成 `1-2-3-7`）。
- 上層來要時立即回 cache，不等當前輪詢跑完一圈。

## 目前狀態
✅ 深鏈 3 層中繼已驗通（`firmware/rp2040_repeater/main.py`，`7→3-7→2-3-7→1-2-3-7`）：前綴、cache 解耦、逾時跳過、全鏈同格式純轉發皆成立（**現行已是 §3.2 的核心文字 0 轉譯管道**）。待補：**自動定址(ENUM 枚舉 master)**、型錄 hot-swap、`up_baud≠down_baud`、CRC 可選逐跳重算。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 B：中繼器韌體（MicroPython/RP2040）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/01_系統架構.md、docs/02_通訊協定.md §3.1、docs/09_軟體架構定案.md、
     docs/06 §J、子專案/B_中繼器韌體.md、firmware/rp2040_repeater/main.py（現有驗通版）。
任務：擴充 RP2040 中繼器——全程核心文字 0xFE、0 轉譯純管道(收下層吐的文字→前綴 path→存 cache，
     不解析 payload)、round-robin、cache+新鮮度、逾時跳過、上層優先回 cache、自動定址(ENUM 枚舉 master)、
     載入型錄供 hot-swap、up_baud≠down_baud、CRC 可選逐跳重算。
     framing/CRC 用 lib/protocol.py，不要改協定契約。完成後 commit + push。
```
