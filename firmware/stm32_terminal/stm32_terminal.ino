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
unsigned long rxCount = 0;   // 診斷：從 bus 收到的 byte 總數

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
  pinMode(LED_BUILTIN, OUTPUT);            // 板載 LED(PC13) 當心跳，肉眼確認活著
  DBG.begin(115200);
  // STM32 USB CDC：等電腦把序列埠打開再印，否則開機訊息會在 USB 列舉前被丟掉（最多等 5 秒）
  unsigned long _t = millis();
  while (!DBG && millis() - _t < 5000) { }
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
      rxCount++;                                     // 診斷：數 bus 收到的 byte
      if (c == '\r') { reqBuf[reqLen] = '\0'; reqLen = 0; return true; }
      if (c == '\n') { continue; }
      if (reqLen < (int)sizeof(reqBuf) - 1) reqBuf[reqLen++] = c;
      start = millis();
    }
  }
  return false;
}

void loop() {
  // 心跳：LED 每 250ms 翻一次，肉眼即可確認板子在跑（與序列埠無關）
  static bool led = false;
  static unsigned long led_t = 0, beat_t = 0;
  if (millis() - led_t > 250) { led_t = millis(); led = !led; digitalWrite(LED_BUILTIN, led); }
  // COM9 診斷：每秒印一次心跳 + 顯示從 bus 收到幾個 byte（rxBytes 有沒有在漲＝有沒有收到東西）
  if (millis() - beat_t > 1000) {
    beat_t = millis();
    DBG.print("[terminal] alive addr="); DBG.print(myAddr);
    DBG.print(" rxBytes="); DBG.println(rxCount);
  }

  if (!readRequest()) return;
  DBG.print("[terminal] rx: "); DBG.println(reqBuf);   // 診斷：收到一整行(含亂碼)就印出來
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
