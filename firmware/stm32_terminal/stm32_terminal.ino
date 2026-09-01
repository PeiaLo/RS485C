/*
 * RS485_C — STM32 末端傳感器（C++ / STM32duino），使用 lib/protocol.*
 * ====================================================================
 * 平台：STM32F103 / F401（Nucleo 或裸板）。末端＝快速驅動 → C++。
 * 被輪詢就回 <位址>|<json>\r（用 rs485c::encode）。半雙工，接 MAX13487(免 DE/RE)。
 *
 *   Bus  : Serial1 (USART1)  接 MAX13487：TX1→DI、RX1←RO、A/B 上 bus
 *   除錯 : Serial            (Nucleo = USART2 = ST-Link VCP)
 *
 * ⚠️ 編譯前：把 lib/protocol.h 與 lib/protocol.cpp 複製到本 sketch 資料夾
 *    （sh firmware/sync_lib.sh）。用 Arduino IDE + STM32 core（STM32duino）。
 * ⚠️ MAX13487 RO 約 5V：STM32 的 RX 腳若是 5V 容忍(FT) 可直接接，否則分壓。
 */
#include "protocol.h"

#define BUS       Serial1
#define DBG       Serial
#define BUS_BAUD  115200
#define REQ_TIMEOUT_MS 300

#define USE_DIP    0
#define FIXED_ADDR 4

#if USE_DIP
// 指撥腳（依你的板改）。⚠️ F401CC 是 48 腳封裝，沒有 PC0~PC12（只有 PC13~15），
// 所以這裡用確定存在的 PA/PB 腳。避開 PA9/PA10(bus)、PA11/PA12(USB)、PA13/PA14(SWD)。
const int ADDR_PINS[6] = {PB12, PB13, PB14, PB15, PA8, PB10};
#define FUNC_PIN PB1
#endif

uint8_t myAddr = FIXED_ADDR;
bool    advancedMode = false;
char    reqBuf[48];
int     reqLen = 0;
float   gTemp = 25.0;

#if USE_DIP
void readDip() {
  uint8_t a = 0;
  for (int i = 0; i < 6; i++) {
    pinMode(ADDR_PINS[i], INPUT_PULLUP);
    if (digitalRead(ADDR_PINS[i]) == LOW) a |= (1 << i);
  }
  pinMode(FUNC_PIN, INPUT_PULLUP);
  bool funcOn = (digitalRead(FUNC_PIN) == LOW);
  myAddr = a;
  advancedMode = !funcOn;
}
#endif

void setup() {
  DBG.begin(115200);
  BUS.begin(BUS_BAUD);
#if USE_DIP
  readDip();
#else
  myAddr = FIXED_ADDR;
#endif
  delay(200);
  DBG.print("[terminal] up, addr=");
  DBG.print(myAddr);
  DBG.print(", mode=");
  DBG.println(advancedMode ? "ADVANCED" : "NORMAL");
}

float readTempC() {
  gTemp += 0.1;
  if (gTemp > 30.0) gTemp = 25.0;
  return gTemp;
}

bool readRequest() {
  unsigned long start = millis();
  while (millis() - start < REQ_TIMEOUT_MS) {      // 溢位安全
    while (BUS.available()) {
      char c = BUS.read();
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
  if (!(strcmp(arg, "ALL") == 0 || atoi(arg) == myAddr)) return;

  char vbuf[10];
  dtostrf(readTempC(), 0, 1, vbuf);                // STM32 newlib：浮點用 dtostrf 最保險
  char json[48];
  strcpy(json, "{\"t\":\"TMP\",\"v\":");
  strcat(json, vbuf);
  strcat(json, ",\"u\":\"C\",\"ok\":1}");
  char addr[6];
  snprintf(addr, sizeof(addr), "%d", myAddr);      // %d 整數 OK（只有 %f 才需特別旗標）

  char frame[72];
  int n = rs485c::encode(addr, json, true /*final→\r*/, frame, sizeof(frame));
  if (n < 0) { DBG.println("[terminal] encode overflow"); return; }

  BUS.write((const uint8_t*)frame, n);
  DBG.print("[terminal] replied: ");
  DBG.print(addr); DBG.print('|'); DBG.println(json);
}
