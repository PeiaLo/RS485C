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
{"t":"TMP","v":26.7,"u":"C","ts":1723100000,"ok":1}
```

| 欄位 | 意義 | 對應設備描述 |
|------|------|--------------|
| `t` | 型別代碼 | = `device.type` |
| 值鍵 | 當下數值 | = `device.measurements[].key`。單值用 `v`；多值用具名鍵 |
| `u` | 單位（選填） | = 對應 measurement 的 `unit` |
| `ts` | 時戳（選填，epoch 秒） | — |
| `ok` | 狀態 1=正常 0=異常（選填） | — |

**單值範例**（TMP）：`{"t":"TMP","v":26.7,"u":"C","ok":1}` → 對應 `measurements:[{"key":"v",...}]`。

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
