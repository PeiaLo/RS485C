/*
 * MAX13487 自檢（Nucleo 版）—— 同 stm32_maxtest 原理，但跑 Nucleo。
 * ====================================================================
 * 單獨判斷一顆 MAX13487 + 你焊的 DI/RO 是好是壞，不需要別的節點。
 * 原理：/RE 拉低時接收器恆開 → 送出的字經自己的收發器繞回 RX（自我回音）。
 *
 * 接線（只接這一顆 MAX）：
 *   Nucleo PA9(D8)→DI(4)、PA10(D2)←RO(1)、VCC(8)=5V、GND(5)、/RE(2)=GND、/SHDN(3)=5V
 *   A(6)/B(7) 建議跨一顆 120Ω（沒有也可能會動）。
 * 板子：Nucleo F401RE；USB support=None；Upload=SWD。開 VCP COM 看結果。
 */
HardwareSerial SerialBus(PA10 /*RX1*/, PA9 /*TX1*/);
#define BUS  SerialBus
#define DBG  Serial
#define BAUD 115200

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  DBG.begin(115200);
  BUS.begin(BAUD);
  delay(300);
  DBG.println("[maxtest] 每 0.5s 送一字，經 MAX 自我回音，看收不收得回。");
  DBG.println("[maxtest] 一直 OK = 這顆 MAX + DI/RO 都好；一直「無」= 這顆或 DI/RO 壞。");
}

uint8_t tx = 'A';
int okCount = 0, failCount = 0;

void loop() {
  while (BUS.available()) BUS.read();
  BUS.write(tx);
  BUS.flush();

  int got = -1;
  unsigned long t = millis();
  while (millis() - t < 50) {
    if (BUS.available()) { got = BUS.read(); break; }
  }
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  if (got == tx) { okCount++; DBG.print("[maxtest] OK   送 "); DBG.print((char)tx); DBG.print(" 收 "); DBG.print((char)got); }
  else if (got < 0) { failCount++; DBG.print("[maxtest] 無   送 "); DBG.print((char)tx); DBG.print(" 收 (無) ← MAX 沒回音"); }
  else { failCount++; DBG.print("[maxtest] 錯   送 "); DBG.print((char)tx); DBG.print(" 收 "); DBG.print((char)got); DBG.print(" ← 訊號髒"); }
  DBG.print("   [OK="); DBG.print(okCount); DBG.print(" 無/錯="); DBG.print(failCount); DBG.println("]");

  tx++; if (tx > 'Z') tx = 'A';
  delay(500);
}
