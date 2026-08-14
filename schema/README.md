# schema — 設備 JSON（模組 E）

兩種 JSON 要分清楚：

1. **設備描述（本資料夾）**：`device.schema.json` 定義「一台設備是什麼、有哪些量測、量程、校正」。
   - 末端傳感器**各自存一份**（自有 JSON）。
   - 中繼器**存所有已知設備的描述**，用來辨識下層。
   - 範例：[examples/temperature_TMP.json](examples/temperature_TMP.json)、[examples/pressure_PRS.json](examples/pressure_PRS.json)
2. **量測 payload（上報用，單行 minified）**：每次被輪詢時回報的即時值，例如
   `{"t":"TMP","v":26.7,"u":"C","ts":1723100000,"ok":1}`
   - `t` 對應設備描述的 `type`。整筆傳輸格式見 [../docs/02_通訊協定.md §3](../docs/02_通訊協定.md)。

> payload 必須單行、不含裸 `\n`/`\r`（框架用），詳見 [02 §2.1](../docs/02_通訊協定.md)。

TODO：加一支驗證腳本（python jsonschema）跑 examples 對 schema。
