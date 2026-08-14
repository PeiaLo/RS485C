# PC 上位機（模組 D · Python）

頂層 master：向第 1 層中繼器要資料、解析、顯示、依條件自動控制。

## 職責
1. **連線**：序列埠（`pyserial`）；未來走模組 F 換 Ethernet/WiFi。
2. **請求排程**：對第 1 層中繼器要 `ALL` 或指定位址（`REQ|<addr|ALL>\r`，見 [../docs/02 §5](../docs/02_通訊協定.md)）。
3. **解析框架** → 更新設備即時值表（key = 完整位址如 `1-32-4`）。
4. **設備資料庫**：用 [../schema/](../schema) 呈現/驗證各設備。
5. **儀表板**：即時值 + 樹狀拓樸 + 新鮮度（依 ts age 上色）。
6. **規則引擎**（[需求 17]）：使用者設條件 → 觸發對「輸出設備」下命令，達成自動控制。
7. Log / 匯出。

## 檔案規劃（待實作）
```
pc/
  main.py            進入點
  link.py            ← 模組 F 介面：RS485Link / EthernetLink 可插拔
  protocol.py        ← 來自模組 C（Python 版）
  scheduler.py       請求排程
  model.py           設備即時值表 + 拓樸
  rules.py           規則引擎（條件→動作）
  ui/                儀表板（先 CLI/tkinter，之後 PyQt6）
  device_db.py       載入 schema/examples
```

## 依賴契約
- [../docs/02_通訊協定.md](../docs/02_通訊協定.md)、[../schema/](../schema)
