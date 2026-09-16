# 子專案 C · 共用協定庫（契約，最先做）

## 目標
把 `\n`/`\r` 框架與位址字串操作，抽成 **C++ 與 Python 兩份行為一致的庫**，當作所有模組的共用契約。

## 範圍
- **要做**：框架編碼/解碼（`\n` 續、`\r` 完整）、`<完整位址>|<payload>` 組裝/解析（payload 對協定庫透明：DESC=JSON / REQ=純值 CSV）、位址字串操作（前綴、切段、驗證）、一組**跨語言測試向量**（同輸入→同輸出）。
- **要做**：**CRC-16/MODBUS**——payload 後接 `*<crc16>`（已定案啟用，見 [docs/02 §3.1](../docs/02_通訊協定.md)）。實作 `crc16()`、設 `CRC_ENABLED=True`，以 `test_vectors.json` 對齊 C++/Python。
- **不做**：任何感測器/UART/硬體邏輯（那是 A/B）。

## 依賴契約
- [docs/02 通訊協定](../docs/02_通訊協定.md)、[docs/03 定址](../docs/03_定址與指撥開關.md)。

## 介面（兩語言需一致）
- `encode(addr_path:str, json_str:str) -> bytes`（產出結尾 `\r` 的完整框）
- `decode(byte_stream) -> (完整? , frame)`（遇 `\n` 續、`\r` 完成）
- `addr_prefix(local:int, child_path:str) -> str`（中繼器前綴用，如 `32` + `4` → `32-4`）
- `addr_split(path:str) -> list[int]`、`addr_valid(path) -> bool`

## 交付物
- `lib/protocol.py`（給 B/D）、`lib/protocol.h` + `lib/protocol.cpp`（給 A）
- `lib/test_vectors.json`（兩邊都跑）
- `lib/README.md`

## 完成定義（DoD）
- C++ 與 Python 跑同一份 `test_vectors.json` 全過。
- Stage 0 的兩支 `.ino` 能改成 `#include` 這份庫、行為不變。

## 目前狀態
`lib/protocol.py`、`lib/protocol.h/.cpp` 已抽出（框架 `\n/\r` + 位址操作，深鏈已驗通）。CRC 目前為預留 stub（`CRC_ENABLED=False`、`crc16()` 待實作）；待實作 CRC-16/MODBUS + 測試向量。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 C：共用協定庫」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/02_通訊協定.md §3.1、docs/09_軟體架構定案.md、docs/03_定址與指撥開關.md、
     子專案/C_協定庫.md、lib/protocol.py、lib/protocol.h/.cpp（現有版）。
任務：完善 lib/protocol.py 與 lib/protocol.h/.cpp 兩份一致實作——payload 對它透明(<位址>|<payload>)、
     啟用 CRC-16/MODBUS(實作 crc16()、CRC_ENABLED=True、*<crc16>)、加 lib/test_vectors.json 跨語言向量。
這是共用契約，其他模組都依賴它——介面定版前先跟我確認。完成後 commit + push。
```
