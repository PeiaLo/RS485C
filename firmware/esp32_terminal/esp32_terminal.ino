/*
 * RS485_C — ESP32 末端傳感器（Arduino/C++），使用 lib/protocol.*
 * ====================================================================
 * 角色：模組 A 的葉節點。被輪詢就回一筆  <位址>|<json>\r（用 rs485c::encode）。
 *
 * 兩台 ESP32 版 Stage 0：兩邊都 3.3V，TTL 直連【免分壓】。
 *   Bus 走 Serial2 (GPIO16=RX2, GPIO17=TX2)；USB Serial 留給除錯。
 *
 * ⚠️ 編譯前：把 lib/protocol.h 與 lib/protocol.cpp 複製到「本 sketch 資料夾」，
 *    Arduino 才編得到（它只編 sketch 資料夾內的 .cpp）。指令見 STAGE0_兩台ESP32.md。
 *    → 這支能編、能跑，就等於在真硬體上驗證了 C++ 版協定庫。
 */
#include "protocol.h"

#define BUS_RX   16
#define BUS_TX   17
#define BUS_BAUD 115200
#define REQ_TIMEOUT_MS 300

#define USE_DIP   0        // 1=讀指撥；0=用寫死位址
#define FIXED_ADDR 4

// 指撥腳（USE_DIP=1）：6 位址 bit + 1 功能 bit，接 GND=ON，內部上拉。
// 這些腳都支援 INPUT_PULLUP（避開 ESP32 strapping 腳 0/2/5/12/15 與只輸入的 34-39）。
const int ADDR_PINS[6] = {32, 33, 25, 26, 27, 14};  // bit0..bit5
#define FUNC_PIN 13

uint8_t myAddr = FIXED_ADDR;
bool    advancedMode = false;      // 功能位 OFF=進階；ON=常規

char  reqBuf[64];
int   reqLen = 0;
float gTemp = 25.0;

void readDip() {
  uint8_t a = 0;
  for (int i = 0; i < 6; i++) {
    pinMode(ADDR_PINS[i], INPUT_PULLUP);
    if (digitalRead(ADDR_PINS[i]) == LOW) a |= (1 << i);   // ON=接GND=1
  }
  pinMode(FUNC_PIN, INPUT_PULLUP);
  bool funcOn = (digitalRead(FUNC_PIN) == LOW);            // ON=常規
  myAddr = a;
  advancedMode = !funcOn;
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(BUS_BAUD, SERIAL_8N1, BUS_RX, BUS_TX);
#if USE_DIP
  readDip();
#else
  myAddr = FIXED_ADDR;
#endif
  delay(200);
  Serial.printf("[terminal] up, addr=%d, mode=%s\n",
                myAddr, advancedMode ? "ADVANCED" : "NORMAL");
}

// 讀感測器 → 攝氏值。預設假的緩變值，沒接感測器也能跑。
float readTempC() {
  gTemp += 0.1;
  if (gTemp > 30.0) gTemp = 25.0;
  return gTemp;
  // 真值：接類比溫度感測器到某 ADC 腳，用 analogRead() 換算後回傳。
}

// 收一則請求直到 \r。收到完整一行回 true（存進 reqBuf）。
bool readRequest() {
  unsigned long start = millis();
  while (millis() - start < REQ_TIMEOUT_MS) {
    while (Serial2.available()) {
      char c = Serial2.read();
      if (c == '\r') { reqBuf[reqLen] = '\0'; reqLen = 0; return true; }
      if (c == '\n') { continue; }
      if (reqLen < (int)sizeof(reqBuf) - 1) reqBuf[reqLen++] = c;
      start = millis();
    }
  }
  return false;
}

void loop() {
  if (!readRequest()) return;

  // 請求格式：REQ|<addr|ALL>。只回應點到自己位址或廣播 ALL 的。
  if (strncmp(reqBuf, "REQ|", 4) != 0) return;
  const char* arg = reqBuf + 4;
  bool hit = (strcmp(arg, "ALL") == 0) || (atoi(arg) == myAddr);
  if (!hit) return;

  // 組單行 json，用 lib 的 encode 出框（結尾自動補 \r）
  float t = readTempC();
  char json[48];
  snprintf(json, sizeof(json), "{\"t\":\"TMP\",\"v\":%.1f,\"u\":\"C\",\"ok\":1}", t);
  char addr[8];
  snprintf(addr, sizeof(addr), "%d", myAddr);

  char frame[80];
  int n = rs485c::encode(addr, json, true /*final→\r*/, frame, sizeof(frame));
  if (n < 0) { Serial.println("[terminal] encode overflow"); return; }

  Serial2.write((const uint8_t*)frame, n);           // frame 已含結尾 \r
  Serial.printf("[terminal] replied: %s|%s\n", addr, json);
  // 進階模式的 CFG 寫入、輸出設備 SET → 第二版再做
}
