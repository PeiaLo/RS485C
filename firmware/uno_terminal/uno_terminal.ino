/*
 * RS485_C — Arduino UNO 末端傳感器（AVR/C++），使用 lib/protocol.*
 * ====================================================================
 * 角色：模組 A 的葉節點（AVR 版）。被輪詢就回 <位址>|<json>\r（用 rs485c::encode）。
 *       這支能編、能跑，就等於在真正的 AVR(2KB SRAM) 上驗證了共用協定庫。
 *
 * AVR 注意事項（本檔已遵守）：
 *   - 不用 String / STL / heap；全程固定 char 緩衝（協定庫本身也是）。
 *   - AVR 的 snprintf 預設不支援 %f → 浮點轉字串用 dtostrf()。
 *   - 常數字串用 F() 放 flash，省 2KB SRAM。
 *   - 匯流排用 SoftwareSerial，讓出硬體 Serial 給 USB 除錯。
 *
 * ⚠️ 編譯前：把 lib/protocol.h 與 lib/protocol.cpp 複製到「本 sketch 資料夾」。
 *
 * ⚠️ Baud：SoftwareSerial 在 UNO 上 115200 不穩 → 本檔用 9600。
 *    當 UNO 一起上 bus 做 Stage 1 時，ESP32 那兩支也要把 BUS_BAUD 改成 9600（全 bus 同 baud）。
 *
 * 接線（Stage 1，真 RS485）：UNO(5V)+MAX13487 直接接；ESP32(3.3V) 的 RO 需分壓。
 *   細節見 firmware/STAGE0_兩台ESP32.md 的 Stage 1 段落。
 */
#include <SoftwareSerial.h>
#include "protocol.h"

#define BUS_RX 10
#define BUS_TX 11
#define BUS_BAUD 9600
#define REQ_TIMEOUT_MS 300

#define USE_DIP   0        // 1=讀指撥；0=用寫死位址
#define FIXED_ADDR 5       // ESP32 末端用 4、UNO 末端用 5（Stage 1 兩台一起）

const uint8_t ADDR_PINS[6] = {2, 3, 4, 5, 6, 7};  // bit0..bit5
#define FUNC_PIN 8

SoftwareSerial bus(BUS_RX, BUS_TX);

uint8_t myAddr = FIXED_ADDR;
bool    advancedMode = false;

char  reqBuf[48];
int   reqLen = 0;
float gTemp = 25.0;

void readDip() {
  uint8_t a = 0;
  for (uint8_t i = 0; i < 6; i++) {
    pinMode(ADDR_PINS[i], INPUT_PULLUP);
    if (digitalRead(ADDR_PINS[i]) == LOW) a |= (1 << i);
  }
  pinMode(FUNC_PIN, INPUT_PULLUP);
  bool funcOn = (digitalRead(FUNC_PIN) == LOW);
  myAddr = a;
  advancedMode = !funcOn;
}

void setup() {
  Serial.begin(115200);          // USB 除錯（硬體 Serial）
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

float readTempC() {
  gTemp += 0.1;
  if (gTemp > 30.0) gTemp = 25.0;
  return gTemp;
}

// 收一則請求直到 \r。收到完整一行回 true。
bool readRequest() {
  unsigned long start = millis();
  while (millis() - start < REQ_TIMEOUT_MS) {     // 溢位安全
    while (bus.available()) {
      char c = bus.read();
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

  if (strncmp(reqBuf, "REQ|", 4) != 0) return;
  const char* arg = reqBuf + 4;
  bool hit = (strcmp(arg, "ALL") == 0) || (atoi(arg) == myAddr);
  if (!hit) return;

  // AVR：浮點轉字串用 dtostrf（不可用 snprintf %f）
  char vbuf[8];
  dtostrf(readTempC(), 0, 1, vbuf);          // 1 位小數
  char json[48];
  // 手動組單行 json（避免 snprintf 浮點）
  strcpy(json, "{\"t\":\"TMP\",\"v\":");
  strcat(json, vbuf);
  strcat(json, ",\"u\":\"C\",\"ok\":1}");

  char addr[6];
  itoa(myAddr, addr, 10);

  char frame[72];
  int n = rs485c::encode(addr, json, true /*final→\r*/, frame, sizeof(frame));
  if (n < 0) { Serial.println(F("[terminal] encode overflow")); return; }

  bus.write((const uint8_t*)frame, n);       // frame 已含結尾 \r
  Serial.print(F("[terminal] replied: "));
  Serial.print(addr);
  Serial.print('|');
  Serial.println(json);
}
