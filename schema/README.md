# schema — 設備 JSON（模組 E）

兩種 JSON 要分清楚：

1. **設備描述（本資料夾）**：`device.schema.json` 定義「一台設備是什麼、有哪些量測、量程、校正」。
   - 末端傳感器**各自存一份**（自有 JSON）。
   - 中繼器**存所有已知設備的描述**，用來辨識下層。
   - 範例：[examples/](examples)（TMP 溫度、PRS 壓力、RH 濕度、DI 數位輸入、ENV 多量測、DO 輸出佔位）。
2. **量測 payload（上報用，單行 minified）**：每次被輪詢時回報的即時值。**本次定版不另做 payload schema**（見下方對應規則），格式定義於此。

## 量測 payload 格式（單行，對應設備描述）

被輪詢時回報的即時值，例如：

```
{"t":"TMP","v":26.7,"u":"C","ok":1,"up":842,"rst":"por"}
```

| 欄位 | 意義 | 對應設備描述 |
|------|------|--------------|
| `t` | 型別代碼 | = `device.type` |
| 值鍵 | 當下數值 | = `device.measurements[].key`。單值用 `v`；多值用具名鍵 |
| `u` | 單位（選填） | = 對應 measurement 的 `unit` |
| `ts` | 時戳（選填，epoch 秒） | — |
| `ok` | 狀態 1=正常 0=異常（選填） | 韌體自我健檢 |
| `up` | uptime 秒（選填，健康監測用） | — |
| `rst` | 上次重置原因（選填，健康監測用） | — |

**單值範例**（TMP）：`{"t":"TMP","v":26.7,"u":"C","ok":1}` → 對應 `measurements:[{"key":"v",...}]`。

## 健康監測 envelope（`ok`/`up`/`rst`，跨設備通用）

這三個是**所有末端通用**的狀態欄位，讓中繼器判斷設備健康狀態機（正常/故障/可疑/消失/剛換/抖動恢復，見 [docs/06 §M](../docs/06_設計決策與待確認.md)）。位址由指撥決定＝設備身分，**不放進 payload**（已在框架前綴裡）。

| 欄位 | 型別 | 說明 |
|------|------|------|
| `ok` | 0/1 | 自我健檢：感測值在範圍/自我測試過＝1，否則 0（活著但故障） |
| `up` | 整數秒 | uptime。**`up` 歸零＝這台剛重開或被換掉**；`up` 連續＝同一台沒死（只是抖動恢復） |
| `rst` | 字串 | 上次重置原因：`por`(上電) `bor`(欠壓/斷電) `iwdg`(看門狗/當機自救) `wwdg` `sft`(軟體) `pin`(按reset) `?`(未知)。來源＝STM32 `RCC_CSR`，開機判定一次 |

> 「剛更換」判定：從「消失」狀態回來 + `up≈0`（DIP 當 ID 時 id 不變，故靠 up）。要真分辨實體換板可另報 STM32 96-bit UID 為 `uid`（選配）。「拔除 vs 座著壞死」網路分不出，需硬體在位偵測腳（見 §M）。

**多值範例**（ENV，多個具名鍵）：`{"t":"ENV","temp":26.7,"rh":45.2,"ok":1}` → 對應 `measurements:[{"key":"temp"},{"key":"rh"}]`。

整筆傳輸格式（`<位址>|<payload>\r`）見 [../docs/02_通訊協定.md §3](../docs/02_通訊協定.md)。payload 必須單行、不含裸 `\n`/`\r`（框架用），詳見 [02 §2.1](../docs/02_通訊協定.md)。

## 驗證

```bash
pip install jsonschema
python validate.py
```

驗證所有 `examples/*.json` 對 `device.schema.json`。目前 6/6 通過。

## 佔位（第二版）
`role:"output"` 的輸出設備欄位（`output.channels`、`output.safe_state`）已在 schema 保留，**韌體/上位機暫不實作**（見 [docs/06 E3](../docs/06_設計決策與待確認.md)）。範例：[examples/output_DO.json](examples/output_DO.json)。
