# 子專案 E · JSON schema + 設備庫（契約）

## 目標
定義「設備身分/能力」的 JSON schema 與設備庫，讓末端存自有 JSON、中繼器辨識下層、PC 顯示。

## 範圍
- **要做**：完善 `device.schema.json`、補足各設備型別範例（溫度/壓力/…）、寫**驗證腳本**跑範例對 schema、定義「量測 payload」單行格式與 schema 對應。
- **要一起想**：未來輸出設備（`role:"output"`）的欄位（通道、斷線安全狀態）先佔位，實作暫緩。
- **不做**：韌體/上位機程式（那是 A/B/D）。

## 依賴契約
- [docs/02 §3 傳輸內容格式](../docs/02_通訊協定.md)、[schema/README.md](../schema/README.md)。

## 交付物
- `schema/device.schema.json`（完善版）
- `schema/examples/*.json`（多種型別）
- `schema/validate.py`（jsonschema 驗證）

## 完成定義（DoD）
- 所有 `examples/*.json` 跑 `validate.py` 全過。
- 量測 payload 的單行格式（如 `{"t":"TMP","v":26.7,...}`）與 schema `type/measurements` 對得起來。

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
