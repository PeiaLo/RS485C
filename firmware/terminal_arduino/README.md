# 末端傳感器韌體（模組 A · Arduino / C++）

單純「驅動 + 傳輸」的葉節點。**不輪詢、不 cache、不拼全路徑**。

## 職責
1. 讀 7 階指撥：6 bit 位址(0–63) + 1 bit 功能（ON 常規 / OFF 進階編輯）。見 [../../docs/03](../../docs/03_定址與指撥開關.md)。
2. 讀感測器 → 組單行 minified JSON payload。
3. 被輪詢時，用共用協定（模組 C 的 C++ 版）回：`<本地位址>|<json>\r`。未送完的分段用 `\n`。
4. 功能位 OFF：進編輯模式，接受寫入自有 JSON/參數（`CFG|...`，見 [../../docs/02 §6](../../docs/02_通訊協定.md)）。

## 目標平台
Arduino UNO（5V，好配 MAX13487）；ESP32 當末端時亦可。

## 檔案規劃（待實作）
```
terminal_arduino/
  terminal_arduino.ino   主迴圈：讀指撥→讀感測器→等要求→回覆
  dip.h/.cpp             指撥讀取
  protocol.h/.cpp        ← 來自模組 C（C++ 版），framing + 位址字串
  sensor_tmp.h/.cpp      感測器驅動（可換型別）
```

## 依賴契約
- [../../docs/02_通訊協定.md](../../docs/02_通訊協定.md)（框架/格式）
- [../../schema/](../../schema)（自有設備 JSON）
