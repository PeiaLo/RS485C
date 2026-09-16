# 子專案 E · JSON schema + 設備庫（契約）

## 目標
定義「設備身分/能力」的 JSON schema 與設備庫，讓末端存自有 JSON、中繼器辨識下層、PC 顯示。

## 範圍
- **要做**：完善 `device.schema.json`（node 身分 + `peripherals[]`：type/binding/driver/calibration/measurements）、補足各設備型別範例、寫**驗證腳本**跑範例對 schema。
- **關鍵**：descriptor 的 `measurements` 順序**一序三用**——① 骨幹私有格式的 CSV 欄位序 ② 末端→中繼器的 **MODBUS 暫存器佈局**（[08 §6.2](../docs/08_末端外設_Driver_韌性架構.md)）③ 外部外設的 **`binding.modbus`** 抓取（[08 §6.1](../docs/08_末端外設_Driver_韌性架構.md)）。schema 要能明確表達順序。見 [docs/02 §3.1](../docs/02_通訊協定.md)。
- **要做**：外部 MODBUS 綁定 `modbus` 區塊（regtype/addr/fc/dtype/word_order/scale）先佔位（見 [docs/08](../docs/08_末端外設_Driver_韌性架構.md)）。
- **要一起想**：輸出設備（`role:"output"`）的欄位（通道、斷線安全狀態）先佔位，實作暫緩。
- **不做**：韌體/上位機程式（那是 A/B/D）。

## 依賴契約
- [docs/02 §3 傳輸內容格式](../docs/02_通訊協定.md)、[schema/README.md](../schema/README.md)。

## 交付物
- `schema/device.schema.json`（完善版）
- `schema/examples/*.json`（多種型別）
- `schema/validate.py`（jsonschema 驗證）

## 完成定義（DoD）
- 所有 `examples/*.json` 跑 `validate.py` 全過。
- descriptor 的 `measurements` 順序能唯一決定骨幹私有 CSV 欄位與末端 MODBUS 暫存器序（如 `[TMP,PRS]` → CSV `25.1,1013` / reg 30001,30002），型別/單位由 schema 還原。

## 目前狀態
`device.schema.json` + 溫度/壓力兩範例已建立（見 [schema/](../schema)）；缺驗證腳本與更多型別。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 E：JSON schema + 設備庫」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/02_通訊協定.md、schema/README.md、schema/device.schema.json、子專案/E_schema.md。
任務：完善 device.schema.json、補多種設備型別範例、寫 schema/validate.py 驗證所有範例。
     輸出設備(role:"output")欄位先佔位不實作。這是共用契約，改動前跟我確認。完成後 commit + push。
```
