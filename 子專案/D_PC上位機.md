# 子專案 D · PC 上位機（Python）

## 目標
PC 端根 master：向第 1 層中繼器要資料、解析、儀表板顯示，並依條件自動控制。

## 範圍
- **要做**：連線（`pyserial`）、請求排程（`REQ|<addr|ALL>`）、解析框架→即時值表（key=完整位址）、設備資料庫（用 schema）、儀表板（先 CLI/tkinter→PyQt6）、規則引擎（條件→動作）、log/匯出。
- **先做骨架**：規則引擎的「輸出動作」先留介面（輸出設備第二版才實作）。
- **傳輸層**：先只做序列埠；之後接模組 F 換 Ethernet/WiFi。

## 依賴契約
- 模組 [C 協定庫](C_協定庫.md)（Python 版）、模組 [E schema](E_schema.md)、[docs/02](../docs/02_通訊協定.md)。

## 交付物
- `pc/`：`main.py`、`link.py`（先 SerialLink，介面為模組 F 預留）、`scheduler.py`、`model.py`、`rules.py`、`device_db.py`、`ui/`，引用 `lib/protocol.py`。

## 完成定義（DoD）
- 接上第 1 層中繼器，能列出各設備即時值 + 完整位址 + 新鮮度。
- 設一條規則（如某位址溫度>門檻）能觸發一個動作（先 log/模擬輸出）。

## 目前狀態
只有骨架 [pc/README.md](../pc/README.md)。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 D：PC 上位機（Python）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/01_系統架構.md、docs/02_通訊協定.md、docs/04_軟體模組拆分.md、
     子專案/D_PC上位機.md、pc/README.md、schema/。
任務：實作 PC 上位機——序列埠連線、請求排程、解析框架、設備即時值表+新鮮度、
     用 schema 的設備資料庫、儀表板、規則引擎(條件→動作，輸出動作先留介面)。
     framing 用 lib/protocol.py；link.py 介面要為之後換 WiFi/Ethernet(模組 F) 預留。完成後 commit + push。
```
