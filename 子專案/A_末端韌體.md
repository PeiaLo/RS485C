# 子專案 A · 末端傳感器韌體（C++ / STM32）

> 平台定案：**STM32 C++**（F411 通用 → F7/H7 重運算，見 [docs/08 §3.1](../docs/08_末端外設_Driver_韌性架構.md)）。原 Arduino/AVR 雛形僅供邏輯參考，不再是目標平台。

## 目標
把末端雛形，做成正式的「驅動＋傳輸」葉節點韌體。

> 傳輸定案（edge=MODBUS）：**末端兩側都走 MODBUS**——對上是中繼器的 MODBUS slave、對下是外部外設的 MODBUS master。私有格式只在中繼器骨幹（見 [docs/02 §3.1](../docs/02_通訊協定.md)、[06 §N](../docs/06_設計決策與待確認.md)）。

## 範圍
- **要做（對上=MODBUS slave）**：**自動定址**（ENUM 鏈序，取代指撥；`ENUM_IN`/`ENUM_OUT` + 枚舉協定，見 [10 §8](../docs/10_自動終端與拓樸.md)）、把**本地 cache 的外設值/健康以 MODBUS 暫存器呈現**給中繼器輪詢（暫存器佈局見 [08 §6.2](../docs/08_末端外設_Driver_韌性架構.md)）；`SET` 走保持暫存器/coil；進階模式走軟體指令 `CFG`。
- **要做（對下=MODBUS master@115200）**：依 descriptor 的 `binding.modbus`（regtype/addr/fc/dtype/word_order/scale）讀外部 PLC 外設，raw→工程值後放進**本地 cache**（見 [08 §6.1](../docs/08_末端外設_Driver_韌性架構.md)）。
- **要做**：driver 登錄表=函式指標（本地感測外設，見 [docs/09 §4](../docs/09_軟體架構定案.md)）、健康 envelope(ok/up/rst)、DESC/JSON（首次自描述 + 10s 心跳）走 `0xFE` 私有 frame、進階模式（功能位 OFF）接受 `CFG` 寫入骨架。
- **要做**：CRC-16/MODBUS（MODBUS 原生 CRC；私有 frame 呼叫模組 C 的 `crc16()`）。
- **先不做**：輸出設備的 `SET` 進階邏輯（第二版）。
- **不做**：樹狀輪詢/前綴拼路徑（那是中繼器 B 的事）；**外設不進樹狀定址**（末端自己 cache + descriptor 索引）。

## 依賴契約
- 模組 [C 協定庫](C_協定庫.md)（C++ 版 framing）、[docs/02](../docs/02_通訊協定.md)、[docs/03](../docs/03_定址與指撥開關.md)、[schema/](../schema)。

## 交付物
- `firmware/stm32_terminal/stm32_terminal.ino`（正式版；`nucleo_terminal.ino` 為開發板變體）
- `board.h`（板級傳輸 config，見 [docs/09](../docs/09_軟體架構定案.md)）、`dip.h/.cpp`、`sensor_*.h/.cpp`（感測器驅動）、引用 `lib/protocol.*`

## 完成定義（DoD）
- 自動定址取得鏈序位址後，中繼器(MODBUS master)FC03/04 讀得到本末端暫存器塊、其他位址不回；換板重枚舉復位同址。
- 接真外設（本地 driver 或外部 MODBUS）能進 cache 回真值；CRC 驗得過；功能位 OFF 進 CFG 模式能收寫入並回 ACK。

## 目前狀態
`firmware/stm32_terminal.ino`、`nucleo_terminal.ino` 已驗通：健康監測(ok/up/rst)、IWDG 看門狗、深鏈末端 addr=7 端到端（現行為私有 JSON 原型）。待補：**MODBUS slave 暫存器呈現、MODBUS master 讀外設、driver 登錄表、board 板級庫**。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 A：末端傳感器韌體（C++/STM32）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/02_通訊協定.md §3.1、docs/08 §3、docs/09_軟體架構定案.md、
     docs/03_定址與指撥開關.md、子專案/A_末端韌體.md、firmware/stm32_terminal.ino（現有版）。
任務：把末端做成正式版——自動定址(ENUM 鏈序)、對上=MODBUS slave(cache 值→暫存器)、對下=MODBUS master@115200
     讀外部外設(binding.modbus)進 cache、driver 登錄表(函式指標)、健康 envelope、DESC/JSON(首次+10s)走 0xFE、
     CFG 骨架、board 板級庫，CRC/framing 用 lib/protocol.*（模組 C）。
     嚴格遵守 docs/02 §3.1 的 edge=MODBUS/core=私有 契約，不要改協定。完成後 commit + push。
```
