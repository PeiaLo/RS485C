/*
 * RS485_C — Stage 0 末端傳感器 (Arduino UNO / AVR)
 * 角色：模組 A 的最小雛形。被輪詢就回一筆  <位址>|<json>\r
 *
 * Stage 0 目的：先用 TTL 直連驗證 \n / \r 協定會動，
 *               確認會動後，再插 MAX13487 換成真正的 RS485。
 * 尚未含 CRC（依決策：先跑通沒有 CRC 的流程，之後只在 buildFrame()
 *            與 master 端解析各加一處即可，見文末註解）。
 *
 * 匯流排(對 ESP32)走 SoftwareSerial；硬體 Serial 留給 USB 除錯列印。
 *
 *   接線 (Stage 0, TTL 直連, 尚無 MAX13487)：
 *     UNO D10 (RX) ──────────────── ESP32 GPIO17 (TX2)   3.3V→5V，UNO 可讀
 *     UNO D11 (TX) ──[ 分壓 ]────── ESP32 GPIO16 (RX2)   5V→3.3V，務必分壓！
 *
 *         分壓 (5V→3.3V)：
 *            UNO D11 ──[1kΩ]──┬── ESP32 GPIO16
 *                            [2kΩ]
 *                             │
 *                            GND
 *
 *     UNO GND ──────────────────── ESP32 GND   (共地必接)
 */

#include <SoftwareSerial.h>

// ---------- 設定 ----------
#define BUS_RX 10
#define BUS_TX 11
#define BUS_BAUD 9600
#define BUS_TIMEOUT_MS 200

#define USE_DIP 0        // 1 = 讀指撥開關；0 = 用寫死位址(還沒接指撥時)
#define FIXED_ADDR 4     // USE_DIP=0 時使用的本地位址

// 指撥腳位 (USE_DIP=1 時)：6 位址 bit + 1 功能 bit
// 撥到 ON 接 GND，用內部上拉；讀到 LOW 視為 1
const uint8_t ADDR_PINS[6] = {2, 3, 4, 5, 6, 7};  // bit0..bit5
#define FUNC_PIN 8

SoftwareSerial bus(BUS_RX, BUS_TX);

uint8_t myAddr = FIXED_ADDR;
bool    advancedMode = false;   // 功能位 OFF=進階/編輯；ON=常規

void readDip() {
  uint8_t a = 0;
  for (uint8_t i = 0; i < 6; i++) {
    pinMode(ADDR_PINS[i], INPUT_PULLUP);
    if (digitalRead(ADDR_PINS[i]) == LOW) a |= (1 << i);  // ON=接GND=1
  }
  pinMode(FUNC_PIN, INPUT_PULLUP);
  bool funcOn = (digitalRead(FUNC_PIN) == LOW);           // ON=常規
  myAddr = a;
  advancedMode = !funcOn;
}

void setup() {
  Serial.begin(115200);   // USB 除錯
  bus.begin(BUS_BAUD);
#if USE_DIP
  readDip();
#else
  myAddr = FIXED_ADDR;
#endif
  Serial.print(F("[terminal] up, addr="));
  Serial.print(myAddr);
  Serial.print(F(", mode="));
  Serial.println(advancedMode ? F("ADVANCED") : F("NORMAL"));
}

// 讀感測器 → 回攝氏值。
// Stage 0 預設用「假的緩變值」，沒接感測器也能跑、方便除錯。
// 要換真感測器：改成讀 A0 的 TMP36/LM35 (見下方註解)。
float readTempC() {
  static float t = 25.0;
  t += 0.1;
  if (t > 30.0) t = 25.0;
  return t;

  // 真值範例 (TMP36 接 A0)：
  // int raw = analogRead(A0);
  // float mv = raw * (5000.0 / 1023.0);
  // return (mv - 500.0) / 10.0;
}

// 組一筆完整資料的內容(不含結尾 \r)： <位址>|<單行json>
String buildFrame() {
  float t = readTempC();
  String json = "{\"t\":\"TMP\",\"v\":";
  json += String(t, 1);
  json += ",\"u\":\"C\",\"ok\":1}";
  return String(myAddr) + "|" + json;
  // 未來加 CRC：回傳 <位址>|<json>*<crc16>，master 端對 "<位址>|<json>" 重算比對
}

// 收一則請求直到 \r。收到完整一行回 true。
// \n 視為「分段、還沒完」→ 繼續等 (Stage 0 的請求是單行，用不到)。
bool readRequest(String &out) {
  static String buf;
  unsigned long start = millis();
  while (millis() - start < BUS_TIMEOUT_MS) {
    while (bus.available()) {
      char c = bus.read();
      if (c == '\r') { out = buf; buf = ""; return true; }   // 完整
      if (c == '\n') { continue; }                            // 未完，續等
      buf += c;
      start = millis();   // 有在收就續命
    }
  }
  return false;
}

void loop() {
  String req;
  if (!readRequest(req)) return;   // 沒收到完整請求就繼續等

  // 解析請求格式： REQ|<addr>   (addr 可為數字或 "ALL")
  int bar = req.indexOf('|');
  if (bar < 0) return;
  String cmd = req.substring(0, bar);
  String arg = req.substring(bar + 1);
  arg.trim();

  if (cmd == "REQ") {
    // 只回應「點到自己位址」或廣播 ALL 的請求
    if (arg == "ALL" || arg.toInt() == myAddr) {
      String frame = buildFrame();
      bus.print(frame);
      bus.print('\r');            // \r = 這筆完整結束
      Serial.print(F("[terminal] replied: "));
      Serial.println(frame);
    }
  }
  // 進階模式的 CFG 寫入 / 輸出設備的 SET 命令 → 第二版再做
}
