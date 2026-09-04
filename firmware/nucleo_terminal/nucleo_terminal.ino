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
#include <IWatchdog.h>        // 卡死自動重開 → 下次開機 rst=iwdg

HardwareSerial SerialBus(PA10 /*RX1*/, PA9 /*TX1*/);
#define BUS   SerialBus
#define DBG   Serial
#define BUS_BAUD 38400   // bus baud（降速；所有節點+探針要一致）。VCP除錯仍115200
#define REQ_TIMEOUT_MS 300
#define WDG_TIMEOUT_US 4000000
// 位址選擇腳（測試「末端在不同階層」用，搬板免重燒）：
//   開路(內部上拉)＝addr5 → 掛在中繼器下層(第二層 bus)，路徑 1-5
//   接 GND        ＝addr2 → 掛在第一層 backbone bus，PC 直接 REQ|2，路徑單段 2
// （這是 7 位 DIP 定址的 1-bit 預覽版）
#define ADDR_SEL_PIN PB0
#define ADDR_OPEN 5
#define ADDR_GND  2

uint8_t myAddr = ADDR_OPEN;    // setup 依 ADDR_SEL_PIN 決定
char  reqBuf[48];
int   reqLen = 0;
float gTemp = 20.0;            // 起始溫度跟第 1 台(25)不同，方便一眼分辨
char  gRst[6] = "?";          // 上次重置原因

// 讀 RCC_CSR 判斷上次為何重置：por=上電 bor=欠壓(斷電) iwdg=看門狗 sft=軟體 pin=按reset。
void detectResetCause() {
  uint32_t csr = RCC->CSR;
  if      (csr & RCC_CSR_IWDGRSTF) strcpy(gRst, "iwdg");
  else if (csr & RCC_CSR_WWDGRSTF) strcpy(gRst, "wwdg");
#ifdef RCC_CSR_BORRSTF
  else if (csr & RCC_CSR_BORRSTF)  strcpy(gRst, "bor");
#endif
  else if (csr & RCC_CSR_PORRSTF)  strcpy(gRst, "por");
  else if (csr & RCC_CSR_SFTRSTF)  strcpy(gRst, "sft");
  else if (csr & RCC_CSR_PINRSTF)  strcpy(gRst, "pin");
  else                             strcpy(gRst, "?");
  RCC->CSR |= RCC_CSR_RMVF;   // 清旗標
}

void setup() {
  detectResetCause();          // 最先做，趁旗標還在（IWDG begin 前）
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ADDR_SEL_PIN, INPUT_PULLUP);
  myAddr = (digitalRead(ADDR_SEL_PIN) == LOW) ? ADDR_GND : ADDR_OPEN;
  DBG.begin(115200);
  BUS.begin(BUS_BAUD);
  delay(200);
  DBG.print("[terminal] up, addr=");
  DBG.print(myAddr);
  DBG.print(myAddr == ADDR_GND ? " (第一層/backbone)" : " (第二層/中繼器下層)");
  DBG.print(", rst="); DBG.println(gRst);
  IWatchdog.begin(WDG_TIMEOUT_US);
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
  IWatchdog.reload();   // 餵狗
  static bool led = false;
  static unsigned long lt = 0;
  if (millis() - lt > 250) { lt = millis(); led = !led; digitalWrite(LED_BUILTIN, led); }

  if (!readRequest()) return;
  if (strncmp(reqBuf, "REQ|", 4) != 0) return;
  const char* arg = reqBuf + 4;
  if (!(strcmp(arg, "ALL") == 0 || atoi(arg) == myAddr)) return;

  char vbuf[10];
  dtostrf(readTempC(), 0, 1, vbuf);
  char ubuf[12];
  snprintf(ubuf, sizeof(ubuf), "%lu", millis() / 1000UL);   // up = uptime 秒
  char json[96];
  strcpy(json, "{\"t\":\"TMP\",\"v\":");
  strcat(json, vbuf);
  strcat(json, ",\"u\":\"C\",\"ok\":1,\"up\":");
  strcat(json, ubuf);
  strcat(json, ",\"rst\":\"");
  strcat(json, gRst);
  strcat(json, "\"}");
  char addr[6];
  snprintf(addr, sizeof(addr), "%d", myAddr);

  char frame[128];
  int n = rs485c::encode(addr, json, true, frame, sizeof(frame));
  if (n < 0) { DBG.println("[terminal] encode overflow"); return; }

  BUS.write((const uint8_t*)frame, n);
  DBG.print("[terminal] replied: ");
  DBG.print(addr); DBG.print('|'); DBG.println(json);
}
