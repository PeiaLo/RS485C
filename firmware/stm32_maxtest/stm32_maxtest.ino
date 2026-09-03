/*
 * MAX13487 自檢（STM32 / STM32duino）
 * ====================================================================
 * 目的：單獨判斷「一顆 MAX13487 板 + 你焊的 DI/RO 接線」是好是壞，
 *       不需要任何其他節點。
 *
 * 原理：MAX13487 半雙工、/RE 拉低時接收器恆開 → 送出的字會經過它自己的
 *       收發器繞一圈回到 RX（自我回音）。收得回 = 這顆 MAX + DI/RO 全 OK。
 *
 * 接線（只接這一顆 MAX，不要接別的節點/USB-485）：
 *     DevEBox PA9 (TX1) → MAX pin4 DI
 *     DevEBox PA10(RX1) ← MAX pin1 RO
 *     MAX pin8 VCC → 5V      pin5 GND → GND
 *     MAX pin2 /RE → GND     pin3 /SHDN → 5V
 *     MAX pin6 A ─┐ **必接**一顆 120Ω！負載驅動器；沒供電的晶片會靠 DI 的 ESD 二極體幽靈供電
 *     MAX pin7 B ─┘
 *
 * 板子設定：BlackPill F401CC、USB support=CDC。燒好開 COM9(115200) 看結果。
 */
#define BUS   Serial1
#define DBG   Serial
#define BAUD  115200

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  DBG.begin(115200);
  unsigned long t = millis();
  while (!DBG && millis() - t < 4000) {}   // 等 USB CDC 打開
  BUS.begin(BAUD);
  DBG.println("[maxtest] 開始：每 0.5s 送一個字，經 MAX 自我回音，看收不收得回。");
  DBG.println("[maxtest] 一直 OK = 這顆 MAX + DI/RO 都好；一直「無」= 這顆或 DI/RO 壞。");
}

uint8_t tx = 'A';
int okCount = 0, failCount = 0;

void loop() {
  while (BUS.available()) BUS.read();       // 清殘留
  BUS.write(tx);
  BUS.flush();                              // 等送完

  int got = -1;
  unsigned long t = millis();
  while (millis() - t < 50) {               // 最多等 50ms 收自己的回音
    if (BUS.available()) { got = BUS.read(); break; }
  }

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  if (got == tx) {
    okCount++;
    DBG.print("[maxtest] OK   送 "); DBG.print((char)tx);
    DBG.print(" 收 "); DBG.print((char)got);
  } else if (got < 0) {
    failCount++;
    DBG.print("[maxtest] 無   送 "); DBG.print((char)tx);
    DBG.print(" 收 (無) ← MAX 沒回音：這顆 MAX 或 DI/RO/供電/致能有問題");
  } else {
    failCount++;
    DBG.print("[maxtest] 錯   送 "); DBG.print((char)tx);
    DBG.print(" 收 "); DBG.print((char)got); DBG.print(" ← 收到但不對(訊號髒)");
  }
  DBG.print("   [OK="); DBG.print(okCount);
  DBG.print(" 無/錯="); DBG.print(failCount); DBG.println("]");

  tx++; if (tx > 'Z') tx = 'A';
  delay(500);
}
