# 子專案 A · 末端傳感器韌體（Arduino / C++）

## 目標
把 Stage 0 的末端雛形，做成正式的「驅動＋傳輸」葉節點韌體。

## 範圍
- **要做**：讀 7 階指撥（6 位址+1 功能）、感測器驅動抽象（可換型別）、組單行 JSON、被輪詢回 `<位址>|<json>\r`、進階模式（功能位 OFF）接受 `CFG` 寫入的骨架。
- **先不做**：CRC（等模組 C 補）、輸出設備的 `SET` 命令（第二版）。
- **不做**：輪詢/cache/拼全路徑（那是中繼器 B 的事）。

## 依賴契約
- 模組 [C 協定庫](C_協定庫.md)（C++ 版 framing）、[docs/02](../docs/02_通訊協定.md)、[docs/03](../docs/03_定址與指撥開關.md)、[schema/](../schema)。

## 交付物
- `firmware/terminal_arduino/terminal_arduino.ino`（正式版）
- `dip.h/.cpp`、`sensor_*.h/.cpp`（感測器驅動）、引用 `lib/protocol.*`

## 完成定義（DoD）
- 撥指撥改位址，master `REQ|<該位址>` 收得到、其他位址不回。
- 接真感測器（如 TMP36/A0）能回真值；功能位 OFF 進入 CFG 模式能收寫入並回 ACK。

## 目前狀態
`firmware/terminal_arduino/terminal_arduino.ino` 已可跑（假溫度、位址預設 4、指撥用 `USE_DIP` 切換）。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 A：末端傳感器韌體（Arduino/C++）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/02_通訊協定.md、docs/03_定址與指撥開關.md、
     子專案/A_末端韌體.md、firmware/terminal_arduino/（現有 Stage0 版）。
任務：把 Stage0 末端擴充成正式版——讀指撥6+1、感測器驅動抽象、CFG 進階模式骨架，
     framing 改用 lib/protocol.*（模組 C）。嚴格遵守 docs/02 的 \n/\r 與 <位址>|<json> 格式，
     不要改協定契約。完成後 commit + push。
```
