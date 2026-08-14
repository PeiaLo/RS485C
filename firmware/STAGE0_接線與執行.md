# Stage 0 · 兩節點對話（接線與執行）

目標：證明 `\n`/`\r` 協定在兩台設備間會動。**先用 TTL 直連**（不接 MAX13487），確認軟體會動，再換成真 RS485。

- 主端：[stage0_esp32_master/stage0_esp32_master.ino](stage0_esp32_master/stage0_esp32_master.ino)（ESP32，Arduino 環境，當中繼器/master）
- 末端：[terminal_arduino/terminal_arduino.ino](terminal_arduino/terminal_arduino.ino)（Arduino UNO，當傳感器）

## 接線（Stage 0，TTL 直連，尚無 MAX13487）

```
   ESP32                                   Arduino UNO
 GPIO17(TX2) ───────────────────────────► D10 (SoftwareSerial RX)   3.3V→5V，直接接
 GPIO16(RX2) ◄──────[ 分壓 5V→3.3V ]────── D11 (SoftwareSerial TX)   5V→3.3V，務必分壓！
     GND     ─────────────────────────────  GND                     共地必接

   分壓 (UNO 5V TX → ESP32 3.3V RX)：
     UNO D11 ──[1kΩ]──┬── ESP32 GPIO16
                     [2kΩ]
                      │
                     GND
```

- **為什麼要分壓**：UNO 的 TX 是 5V，直接灌進 ESP32 的 3.3V RX 腳會超壓傷板。1k/2k 分壓把 5V 降到約 3.3V。
- 反方向（ESP32 3.3V TX → UNO RX）通常可直接接，UNO 判 HIGH 的門檻約 3V，3.3V 打得動；若不穩再加準位轉換。
- 兩塊板各自用自己的 USB 供電即可，**GND 一定要共接**。

## 執行步驟

1. 用 Arduino IDE 開 `terminal_arduino.ino`，選板子 = Arduino UNO，燒錄。打開序列埠監控 115200 → 應看到 `[terminal] up, addr=4, mode=NORMAL`。
2. 開 `stage0_esp32_master.ino`，選板子 = 你的 ESP32 開發板，燒錄。打開它的序列埠監控 115200。
3. 依上面接線把兩塊板接起來（含分壓與共地）。
4. ESP32 監控每秒應印出：
   ```
   [master] addr 4 -> 4|{"t":"TMP","v":25.3,"u":"C","ok":1}
   ```
5. **測逾時**：把 UNO 的 TX 線（D11）拔掉 → ESP32 應印 `addr 4 -> TIMEOUT (stale)`；接回 → 恢復正常。這就驗證了「等 `\r` 逾時就跳過/標 stale」。

## 過關條件

- [ ] 連續每秒穩定收到完整一筆（末尾 `\r` 被正確辨識）。
- [ ] 拔線能被判為 TIMEOUT，不會卡死。
- [ ] UNO 換 `USE_DIP 1` 並接上指撥後，改撥位址，master 端的 `REQ|4` 要改成對應位址才收得到（驗證定址）。

## 下一步（Stage 0 → Stage 1）

1. 把 TTL 直連換成 **各一顆 MAX13487** 的真 RS485（A/B 兩線 + 兩端 120Ω 終端 + 一處 failsafe 偏壓）。UNO(5V) 直接接 MAX13487；ESP32(3.3V) 的 RO 記得分壓（見 [docs/06 D2](../docs/06_設計決策與待確認.md)）。
2. 把 master 改寫成 **MicroPython 版中繼器**，`children` 變多台，加 `cache` dict 與 round-robin（Stage 1）。
3. 之後再把 `\n`/`\r` 那段抽成共用協定庫（模組 C），並補上 CRC。
