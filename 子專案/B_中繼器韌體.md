# 子專案 B · 中繼器韌體（MicroPython / ESP32）

## 目標
把 Stage 0 的 Arduino 版 master 雛形，改寫成正式的 MicroPython 中繼器：對上是 slave、對下是 master。

## 範圍
- **要做**：雙 UART（上層 slave + 下層 master）、round-robin 輪詢、cache dict（含新鮮度）、逾時跳過、**前綴自己的本地位址**向上拼路徑、載入已知設備 JSON 庫、上層要求優先回 cache。
- **先不做**：CRC、WiFi 傳輸（那是 F）、輸出 `SET` 下行（第二版）。

## 依賴契約
- 模組 [C 協定庫](C_協定庫.md)（Python 版）、模組 [E schema](E_schema.md)、[docs/02](../docs/02_通訊協定.md)、[docs/03](../docs/03_定址與指撥開關.md)。

## 交付物
- `firmware/repeater_micropython/`：`main.py`、`poller.py`、`cache.py`、`addr.py`、`device_db.py`，引用 `lib/protocol.py`。

## 完成定義（DoD）
- 一台中繼器輪詢多台末端，cache 正確；拔掉一台→該格 stale、其餘照跑。
- 末端資料經中繼器後被前綴成 `<本地>-<末端>`（Stage 2 再多一層變 `1-32-4`）。
- 上層來要時立即回 cache，不等當前輪詢跑完一圈。

## 目前狀態
只有 Stage 0 的 **Arduino 版** 單末端 master（`firmware/stage0_esp32_master/`）可參考邏輯；正式版用 MicroPython 重寫並支援多末端+cache。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 B：中繼器韌體（MicroPython/ESP32）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/01_系統架構.md、docs/02_通訊協定.md、docs/03_定址與指撥開關.md、
     子專案/B_中繼器韌體.md、firmware/stage0_esp32_master/（Arduino 版參考邏輯）、
     firmware/repeater_micropython/README.md。
任務：用 MicroPython 寫正式中繼器——雙 UART、round-robin 輪詢、cache dict+新鮮度、
     逾時跳過、前綴本地位址拼路徑、載入設備 JSON 庫、上層優先回 cache。framing 用 lib/protocol.py。
     不要改協定契約。完成後 commit + push。
```
