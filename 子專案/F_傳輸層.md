# 子專案 F · 傳輸層抽象（擴充，之後做）

## 目標
把「連結」抽象化，讓某一段從 RS485 換成 Ethernet/WiFi 時，協定/定址/cache 完全不用改。

## 範圍
- **要做**：定義 `Link` 介面（`send_bytes`/`recv_bytes`）；提供 `RS485Link` 與 `WiFiLink`/`EthernetLink`（TCP/UDP 承載同樣的 `\n`/`\r` bytes）；頂層中繼器與 PC 只依賴介面。
- **用途**：① 頂層對 PC 頻寬紓解 ② 拉不了線的分支橋接（見 [docs/06 §G](../docs/06_設計決策與待確認.md)）。
- **不做**：改動框架/協定內容（媒介無關，bytes 照舊）。

## 依賴
- 模組 [B 中繼器](B_中繼器韌體.md)、[D PC](D_PC上位機.md) 已能用序列埠跑起來後才做。

## 交付物
- `pc/link.py` 增 `WiFiLink`/`EthernetLink`；ESP32 端「橋接中繼器」韌體（下 RS485、上 WiFi）。

## 完成定義（DoD）
- 把某一段連結從 RS485 換成 WiFi，上層拿到的資料與位址完全一致，程式其餘不動。
- WiFi 斷線時該子樹仍本地輪詢+cache，重連後把 cache 補上。

## 目前狀態
僅概念（見 [docs/04 模組 F](../docs/04_軟體模組拆分.md)）。等 B、D 跑通、或真的遇到頻寬/跨棟需求再開。

## 📋 開新對話貼這段
```
我要開發 RS485_C 專案的「模組 F：傳輸層抽象（RS485 ↔ WiFi/Ethernet）」。
Repo：https://github.com/PeiaLo/RS485C（本機 C:\Users\apial\Desktop\RS485_C）。先 git pull。
先讀：README.md、docs/01_系統架構.md §5、docs/04_軟體模組拆分.md(模組F)、docs/06_設計決策與待確認.md §G、
     子專案/F_傳輸層.md、pc/README.md。
任務：定義 Link 介面並實作 RS485Link + WiFiLink(TCP/UDP 承載 \n/\r bytes)，
     讓某段連結可插拔切換而協定/定址/cache 不變；含 ESP32 橋接中繼器(下RS485、上WiFi)。完成後 commit + push。
```
