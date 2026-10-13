# 子專案 C · 共用協定庫（契約，最先做）

## 目標
把 `\n`/`\r` 框架與位址字串操作，抽成 **C++ 與 Python 兩份行為一致的庫**，當作所有模組的共用契約。

> 契約定案（edge=MODBUS / core=私有）：協定庫要同時撐兩種框架——① **私有格式 `0xFE`**（骨幹，`<path>|<payload>*<crc>\r`）② **MODBUS RTU**（葉段/外設，addr+FC+data+CRC16）。第一 byte 判別用哪個。見 [docs/02 §3.1](../docs/02_通訊協定.md)、[06 §N](../docs/06_設計決策與待確認.md)。

## 範圍
- **要做（第一 byte 判別）**：讀入首 byte → `1-63`/`0xFF`=MODBUS、`0xFE`=私有，分派對應解析器。
- **要做（私有格式）**：`\n` 續/`\r` 完整框架、`<path>|<payload>` 組裝/解析（payload 透明：CSV 值 / JSON）、`*<crc16>` **`\r` 前驗**、位址字串操作（前綴、切段、驗證 1–63，**path 到節點**）、跨語言測試向量。
- **要做（MODBUS RTU）**：組/解 MODBUS 幀（addr/FC/data/CRC16-LH）、FC03/04/06/16 helper、廣播 `0xFF`。給末端(slave)、中繼器葉段(master) 共用。
- **要做（自動定址枚舉）**：`enumerate` 請求/回應框架 + 本地位址指派 helper（master 配序號、slave 回應）；配合 `ENUM` 腳時序（硬體腳在 A/B，協定庫只管訊息）。見 [10 §8](../docs/10_自動終端與拓樸.md)。
- **要做（CRC 可選）**：**預留** `*<crc>` 槽與 `crc16()`，但 **`CRC_ENABLED` 預設 False**（初期不設、先跑通；後可加、不影響硬體）。開時 CRC-16/MODBUS、`test_vectors.json` 對齊 C++/Python（私有與 MODBUS 同多項式）。`feat/modbus` 已有實作可併入。
- **不做**：任何感測器/UART/硬體邏輯（那是 A/B）。

## 依賴契約
- [docs/02 通訊協定](../docs/02_通訊協定.md)、[docs/03 定址](../docs/03_定址與指撥開關.md)。

## 介面（兩語言需一致）
- `classify(first_byte) -> {MODBUS | PRIVATE}`（第一 byte 判別）
- 私有：`encode_priv(addr_path, payload) -> bytes`（`0xFE …*<crc>\r`）、`decode_priv(stream) -> (完整?, frame, crc_ok)`
- MODBUS：`mb_build(addr, fc, data) -> bytes`、`mb_parse(frame) -> (addr, fc, data, crc_ok)`
- `addr_prefix(local:int, child_path:str) -> str`（中繼器前綴，如 `32`+`4`→`32-4`）、`addr_split`、`addr_valid`（1–63）
- `crc16(bytes) -> int`（MODBUS 多項式，私有與 MODBUS 共用）

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
任務：完善 lib/protocol.py 與 lib/protocol.h/.cpp——第一 byte 判別、私有格式(0xFE <path>|<payload>*<crc>\r)、
     MODBUS RTU 幀(addr/FC/data/CRC16)、CRC 可選(crc16()+*<crc>槽預留、CRC_ENABLED 預設 False)、lib/test_vectors.json 跨語言向量。
這是共用契約，其他模組都依賴它——介面定版前先跟我確認。完成後 commit + push。
```
