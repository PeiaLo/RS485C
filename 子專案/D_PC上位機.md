# 子專案 D · PC 上位機（Python）

## 目標
PC 端根 master：向第 1 層中繼器要資料、解析、儀表板顯示，並依條件自動控制。

## 範圍
- **要做**：連線（`pyserial`）、請求排程（`REQ|<addr|ALL>`，位址到**節點**）、解析**私有格式 `0xFE`** 框架→即時值表（key=節點路徑；值為 descriptor 定序 CSV，靠 descriptor 還原型別/單位；`\r` 前驗 CRC）、設備資料庫/型錄（用 schema）、儀表板（先 CLI/tkinter→PyQt6）、規則引擎（條件→動作，觸發 `SET` 下行需 ACK）、log/匯出。
- **對客戶北向**：可暴露 **MODBUS-TCP + MQTT**（collector 模式，見 [docs/09](../docs/09_軟體架構定案.md)）。
- **先做骨架**：規則引擎的「輸出動作」先留介面（輸出設備第二版才實作）。
- **傳輸層**：先只做序列埠；之後接模組 F 換 Ethernet。

## 依賴契約
- 模組 [C 協定庫](C_協定庫.md)（Python 版）、模組 [E schema](E_schema.md)、[docs/02](../docs/02_通訊協定.md)。

## 交付物
- `pc/`：`main.py`、`link.py`（先 SerialLink，介面為模組 F 預留）、`scheduler.py`、`model.py`、`rules.py`、`device_db.py`、`ui/`，引用 `lib/protocol.py`。

## 完成定義（DoD）
- 接上第 1 層中繼器，能列出各設備即時值 + 完整位址 + 新鮮度。
- 設一條規則（如某位址溫度>門檻）能觸發一個動作（先 log/模擬輸出）。

## 目前狀態
已有 [rs485_monitor.py](../rs485_monitor.py)（多埠監控，PC 監控基礎）、[rs485_probe.py](../rs485_probe.py)（單點查詢，深鏈已驗通）；正式上位機（排程/儀表板/規則引擎）待實作，骨架見 [pc/README.md](../pc/README.md)。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 D：PC 上位機（Python）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/01_系統架構.md、docs/02_通訊協定.md、docs/04_軟體模組拆分.md、
     子專案/D_PC上位機.md、pc/README.md、schema/。
任務：實作 PC 上位機——序列埠連線、請求排程、解析框架、設備即時值表+新鮮度、
     用 schema 的設備資料庫、儀表板、規則引擎(條件→動作，輸出動作先留介面)。
     framing 用 lib/protocol.py；link.py 介面要為之後換 Ethernet(模組 F, W5500) 預留。完成後 commit + push。
```
