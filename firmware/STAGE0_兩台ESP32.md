# Stage 0 (兩台 ESP32) → Stage 1 (加 UNO)

用你手上的 **2×ESP32 + 1×UNO**，分兩步：先用兩台 ESP32 驗協定（最省事），再加 UNO 上真 RS485 做多末端。

三支 sketch 都 `#include` 已定版的共用協定庫 `lib/protocol.*`——**能編、能通，就等於在真硬體上驗證了 C++ 協定庫**（補掉「C++ 版沒被編譯過」的缺口）。

| sketch | 角色 | 平台 |
|--------|------|------|
| [esp32_master/](esp32_master/esp32_master.ino) | 主端 / 中繼器雛形，輪詢 `{4,5}` | ESP32 (Arduino) |
| [esp32_terminal/](esp32_terminal/esp32_terminal.ino) | 末端 addr **4** | ESP32 (Arduino) |
| [uno_terminal/](uno_terminal/uno_terminal.ino) | 末端 addr **5** | UNO (AVR) |

## 編譯前一定要做：同步 lib 副本

Arduino 只編 sketch 資料夾內的 `.cpp`，所以每個 sketch 都要一份 `lib/protocol.*`。已幫你複製好；若之後 `lib/` 有更新，或在別台 clone 後，重跑：

```bash
sh firmware/sync_lib.sh
```

PowerShell 版：
```
Copy-Item lib\protocol.* firmware\esp32_master\; Copy-Item lib\protocol.* firmware\esp32_terminal\; Copy-Item lib\protocol.* firmware\uno_terminal\
```

（這些副本已 gitignore，`lib/` 才是單一真相。）

---

## Stage 0 — 兩台 ESP32（TTL 直連，免分壓、免收發器）

兩台都 3.3V，可直接對接。**只驗 master ↔ 1 個末端（點對點）**。

```
  ESP32-A (master)                    ESP32-B (terminal addr 4)
   GPIO17 (TX2) ───────────────────►  GPIO16 (RX2)
   GPIO16 (RX2) ◄───────────────────  GPIO17 (TX2)
      GND       ─────────────────────  GND        (共地必接)
```

- baud：兩支 `BUS_BAUD` 都是 **115200**（ESP32 硬體 Serial2 沒問題）。
- 各自 USB 供電，GND 必接。

**步驟**：
1. Arduino IDE 開 `esp32_terminal.ino`，選你的 ESP32 板，燒錄 → 監控 115200 應見 `[terminal] up, addr=4, mode=NORMAL`。
2. 開 `esp32_master.ino`，燒到另一台 ESP32 → 監控 115200。
3. 依上面接兩條訊號線 + 共地。
4. master 監控應每秒出現：
   ```
   [master] addr 4 -> 4|{"t":"TMP","v":25.3,"u":"C","ok":1}
   [master] addr 5 -> TIMEOUT (stale)      ← 還沒接 addr 5，正常
   ```
   → addr 4 收得到 = 協定 + C++ 庫在真硬體上通了；addr 5 逾時 = 逾時機制正確。

---

## Stage 1 — 加 UNO 當第 2 個末端（真 RS485 multidrop）

3 台在同一條 bus = 真正多點，**需要每台一顆 MAX13487**（TTL 不能把多支 TX 併在一起）。

```
              ┌─────────── RS485 A/B 兩線（bus）───────────┐
   [ESP32 master]      [ESP32 term addr4]      [UNO term addr5]
   +MAX13487           +MAX13487               +MAX13487
   (3.3V,RO需分壓)     (3.3V,RO需分壓)          (5V,直接接)
   bus 兩端各 120Ω 終端；至少一處 failsafe 偏壓（上拉A、下拉B）
```

**要改的地方**：
1. **全 bus 同 baud**：UNO 用 SoftwareSerial，115200 不穩 → 把**三支的 `BUS_BAUD` 都改成 9600**（`uno_terminal` 已是 9600；兩支 ESP32 改成 9600）。
2. **接收發器**：每台 MCU 的 TX→DI、RX←RO 接到各自的 MAX13487，A/B 併到同一條 bus。
   - UNO(5V)+MAX13487(5V)：直接接。
   - ESP32(3.3V)：MAX13487 的 RO 輸出約 5V → **RO→ESP32 RX 之間要 1k/2k 分壓**（見 [docs/06 D2](../docs/06_設計決策與待確認.md)）。
3. `esp32_master` 的 `children` 已是 `{4, 5}` → 兩台末端都會被輪詢。

**預期**：master 監控兩台都 fresh；拔掉其中一台 → 該位址 `TIMEOUT (stale)`、另一台照收（Stage 1 的多末端 + 逾時跳過驗證完成）。

---

## 疑難排解

- **全部 TIMEOUT**：多半是 TX/RX 沒交叉、沒共地、或 baud 沒對齊。
- **收到亂碼**：Stage 1 若 ESP32 沒對 RO 分壓（5V 灌進 3.3V）；或 baud 不一致。
- **UNO 溫度顯示怪**：AVR 的 `snprintf` 不支援 `%f`；本 sketch 已改用 `dtostrf`，別改回 `snprintf("%.1f")`。
- **編譯找不到 `protocol.h`**：忘了跑 `sync_lib.sh`。
