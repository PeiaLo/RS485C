# 子專案 B · 中繼器韌體（MicroPython / RP2040）

> 平台定案：**RP2040**（雙 UART 是選它的主因；ESP32 去陸製排除、NUCLEO_F401RE MicroPython 僅 1 UART 淘汰，見 [docs/06 §J](../docs/06_設計決策與待確認.md)）。三分類：純輸送 / 高速傳送(W5500) / 計算與驅動(RPi)。

## 目標
寫正式的 MicroPython 中繼器：對上是 slave、對下是 master。

> 傳輸定案（edge=MODBUS / core=私有）：**葉段中繼器 down=MODBUS master**（FC03/04 讀末端暫存器），**up=私有格式**（骨幹）；**骨幹中繼器兩側都是私有格式**、對子中繼器 payload 純轉發。見 [docs/02 §3.1](../docs/02_通訊協定.md)、[06 §N](../docs/06_設計決策與待確認.md)。

## 範圍
- **要做（葉段）**：down 當 **MODBUS master** 輪詢末端（FC03/04 讀整塊暫存器）→ 照 descriptor 還原 → **轉成私有格式 `0xFE`**、前綴本地位址、存 cache。
- **要做（骨幹）**：up/down 皆私有格式；round-robin、cache dict+新鮮度、逾時跳過、前綴拼路徑、上層優先回 cache；子中繼器來的私有 payload **純轉發**（CRC 逐跳重算、age/健康 envelope 放框架外，見 [docs/09](../docs/09_軟體架構定案.md)）。
- **要做（自動定址 master）**：對下層當**枚舉 master**——廣播 `enumerate`、依 `ENUM` 鏈序逐台配本地位址（1,2,3…）、換板重枚舉復位；本地 path = 父層 path + 子鏈序（見 [10 §8](../docs/10_自動終端與拓樸.md)、[03](../docs/03_定址與指撥開關.md)）。DIP 已拿掉。
- **要做**：載入型錄(catalog)供 hot-swap（位址自動 + 型錄 descriptor 給身分 → 換同款即自動配置）；`up_baud ≠ down_baud`（葉段 MODBUS 慢、骨幹私有快）。
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
✅ 深鏈 3 層中繼已驗通（`firmware/rp2040_repeater/main.py`，`7→3-7→2-3-7→1-2-3-7`）：前綴、cache 解耦、逾時跳過皆成立（現行為私有 JSON 原型、全鏈同格式）。待補：**葉段 down 改 MODBUS master 讀末端暫存器→轉私有格式**、骨幹私有純轉發、型錄 hot-swap、CRC 逐跳重算、`up_baud≠down_baud`。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 B：中繼器韌體（MicroPython/RP2040）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/01_系統架構.md、docs/02_通訊協定.md §3.1、docs/09_軟體架構定案.md、
     docs/06 §J、子專案/B_中繼器韌體.md、firmware/rp2040_repeater/main.py（現有驗通版）。
任務：擴充 RP2040 中繼器——葉段 down=MODBUS master 讀末端暫存器→轉私有格式 0xFE、
     骨幹 up/down 私有格式、round-robin、cache+新鮮度、逾時跳過、前綴本地位址、載入型錄供 hot-swap、
     上層優先回 cache、子中繼器 payload 純轉發(CRC 逐跳重算、age/健康放框架外)、up_baud≠down_baud。
     framing/CRC 用 lib/protocol.py，不要改協定契約。完成後 commit + push。
```
