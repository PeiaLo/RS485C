# 中繼器韌體（模組 B · MicroPython / ESP32）

分支節點：**對上層是 slave、對下層是 master**。用 MicroPython 善用 `dict`/`list`。

## 職責
1. **雙 UART**：`uart_up`（上層 bus，被要求才回）+ `uart_down`（下層 bus，主動輪詢）。
2. **輪詢迴圈**：round-robin 問下層每個孩子；收到 `\r` 存 cache，等 `\r` 逾時就跳過。
3. **cache**：`dict{本地位址: {"data":..., "ts":..., "fresh":bool}}`。
4. **前綴**：孩子資料前面接上「自己的本地位址 + `-`」，向上拼路徑（見 [../../docs/03 §3](../../docs/03_定址與指撥開關.md)）。
5. **上層優先**：`uart_up` 一有要求就回 cache，不等當前輪詢跑完整圈。
6. **JSON 庫**：載入所有已知設備描述以辨識下層。

## 目標平台
ESP32（MicroPython）。3.3V — 與 5V MAX13487 的準位注意事項見 [../../docs/05](../../docs/05_測試架構建議.md) 與 [../../docs/06 D2](../../docs/06_設計決策與待確認.md)。

## 檔案規劃（待實作）
```
repeater_micropython/
  main.py            開機起輪詢迴圈 + 上層服務
  poller.py          round-robin 輪詢 + 逾時
  cache.py           cache dict + 新鮮度
  protocol.py        ← 來自模組 C（Python 版），framing + 位址字串
  addr.py            指撥讀取 + 前綴拼接
  device_db.py       載入所有已知設備 JSON
```

## 依賴契約
- [../../docs/02_通訊協定.md](../../docs/02_通訊協定.md)、[../../docs/03_定址與指撥開關.md](../../docs/03_定址與指撥開關.md)
- [../../schema/](../../schema)
