# 子專案 A · 末端傳感器韌體（C++ / STM32）

> 平台定案：**STM32 C++**（F411 通用 → F7/H7 重運算，見 [docs/08 §3.1](../docs/08_末端外設_Driver_韌性架構.md)）。原 Arduino/AVR 雛形僅供邏輯參考，不再是目標平台。

## 目標
把末端雛形，做成正式的「驅動＋傳輸」葉節點韌體。

## 範圍
- **要做**：讀 7 階指撥（6 位址+1 功能）、感測器驅動抽象（**driver 登錄表=函式指標**，見 [docs/09 §4](../docs/09_軟體架構定案.md)）、按 descriptor 指向值槽**串出純值 CSV**、被輪詢回 `<位址>|<值CSV>*<crc>\r`、健康 envelope(ok/up/rst)放框架外、進階模式（功能位 OFF）接受 `CFG` 寫入的骨架。
- **要做**：CRC-16/MODBUS（呼叫模組 C 的 `crc16()`）。
- **先不做**：輸出設備的 `SET` 命令（第二版）。
- **不做**：輪詢/cache/拼全路徑（那是中繼器 B 的事）。

## 依賴契約
- 模組 [C 協定庫](C_協定庫.md)（C++ 版 framing）、[docs/02](../docs/02_通訊協定.md)、[docs/03](../docs/03_定址與指撥開關.md)、[schema/](../schema)。

## 交付物
- `firmware/stm32_terminal/stm32_terminal.ino`（正式版；`nucleo_terminal.ino` 為開發板變體）
- `board.h`（板級傳輸 config，見 [docs/09](../docs/09_軟體架構定案.md)）、`dip.h/.cpp`、`sensor_*.h/.cpp`（感測器驅動）、引用 `lib/protocol.*`

## 完成定義（DoD）
- 撥指撥改位址，master `REQ|<該位址>` 收得到、其他位址不回。
- 接真感測器能回真值；CRC 驗得過；功能位 OFF 進入 CFG 模式能收寫入並回 ACK。

## 目前狀態
`firmware/stm32_terminal.ino`、`nucleo_terminal.ino` 已驗通：健康監測(ok/up/rst)、IWDG 看門狗、BUS_BAUD 38400、深鏈末端 addr=7 端到端。待補：driver 登錄表、純值 CSV、board 板級庫。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 A：末端傳感器韌體（C++/STM32）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/02_通訊協定.md §3.1、docs/08 §3、docs/09_軟體架構定案.md、
     docs/03_定址與指撥開關.md、子專案/A_末端韌體.md、firmware/stm32_terminal.ino（現有版）。
任務：把末端做成正式版——讀指撥6+1、driver 登錄表(函式指標)、按 descriptor 串出純值 CSV+CRC、
     健康 envelope、CFG 進階模式骨架、board 板級庫，framing/CRC 用 lib/protocol.*（模組 C）。
     嚴格遵守 docs/02 §3.1 的 DESC=JSON/REQ=純值CSV 契約，不要改協定。完成後 commit + push。
```
