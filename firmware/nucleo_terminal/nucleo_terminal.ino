/*
 * RS485_C — Nucleo 末端傳感器（C++ / STM32duino），使用 lib/protocol.*
 * ====================================================================
 * 第 2 台末端（addr 5），跑在 Nucleo-F401RE。Nucleo 內建 ST-Link 好燒、VCP 序列埠可靠。
 *   Bus : USART1 (PA9 TX1 / PA10 RX1) → MAX13487
 *   除錯: Serial (板載 ST-Link VCP)
 *
 * 板子設定：Board = Nucleo-64 → Nucleo F401RE；USB support = None；
 *          Upload = STM32CubeProgrammer (SWD)（板載 ST-Link，插 USB 直接上傳）。
 * ⚠️ 編譯前 sync_lib 複製 protocol.h/.cpp 進本資料夾。
 * ⚠️ Nucleo 變體無 Serial1 → 自建 HardwareSerial(PA10 RX, PA9 TX)。
 *
 * 接線（MAX13487，先用 stm32_maxtest 驗過是好的那顆）：
 *   Nucleo PA9(D8)→DI(4)、PA10(D2)←RO(1)、VCC(8)=5V、GND(5)、/RE(2)=GND、/SHDN(3)=5V
 *   A(6)/B(7) 併到同一條 bus（跟第 1 台 A-A、B-B）；bus 兩端 120Ω、一處 560Ω 偏壓。
 */
#include "protocol.h"

HardwareSerial SerialBus(PA10 /*RX1*/, PA9 /*TX1*/);
#define BUS   SerialBus
#define DBG   Serial
#define BUS_BAUD 115200
#define REQ_TIMEOUT_MS 300
#define MY_ADDR 5              // 第 2 台 = 位址 5

char  reqBuf[48];
int   reqLen = 0;
float gTemp = 20.0;            // 起始溫度跟第 1 台(25)不同，方便一眼分辨

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  DBG.begin(115200);
  BUS.begin(BUS_BAUD);
  delay(200);
  DBG.print("[terminal] up, addr=");
  DBG.println(MY_ADDR);
}

float readTempC() {
  gTemp += 0.1;
  if (gTemp > 25.0) gTemp = 20.0;
  return gTemp;
}

bool readRequest() {
  unsigned long start = millis();
  while (millis() - start < REQ_TIMEOUT_MS) {
    while (BUS.available()) {
      char c = BUS.read();
      if (c == '\r') { reqBuf[reqLen] = '\0'; reqLen = 0; return true; }
      if (c == '\n') continue;
      if (reqLen < (int)sizeof(reqBuf) - 1) reqBuf[reqLen++] = c;
      start = millis();
    }
  }
  return false;
}

void loop() {
  static bool led = false;
  static unsigned long lt = 0;
  if (millis() - lt > 250) { lt = millis(); led = !led; digitalWrite(LED_BUILTIN, led); }

  if (!readRequest()) return;
  if (strncmp(reqBuf, "REQ|", 4) != 0) return;
  const char* arg = reqBuf + 4;
  if (!(strcmp(arg, "ALL") == 0 || atoi(arg) == MY_ADDR)) return;

  char vbuf[10];
  dtostrf(readTempC(), 0, 1, vbuf);
  char json[48];
  strcpy(json, "{\"t\":\"TMP\",\"v\":");
  strcat(json, vbuf);
  strcat(json, ",\"u\":\"C\",\"ok\":1}");
  char addr[6];
  snprintf(addr, sizeof(addr), "%d", MY_ADDR);

  char frame[72];
  int n = rs485c::encode(addr, json, true, frame, sizeof(frame));
  if (n < 0) { DBG.println("[terminal] encode overflow"); return; }

  BUS.write((const uint8_t*)frame, n);
  DBG.print("[terminal] replied: ");
  DBG.print(addr); DBG.print('|'); DBG.println(json);
}
