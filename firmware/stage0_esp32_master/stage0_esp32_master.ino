/*
 * RS485_C — Stage 0 主輪詢端 (ESP32, Arduino 環境)
 * 角色：模組 B(中繼器) 的最小雛形 —— 定時輪詢下層末端、解析 \n/\r、印出結果、逾時標示 stale。
 *
 * 為什麼 Stage 0 用 Arduino 環境而不是 MicroPython：
 *   兩邊都在 Arduino IDE，零額外設定就能燒錄、當天看到結果(符合「先跑通」)。
 *   等 Stage 1 要做「一台中繼器接多個末端 + cache」時，再把這支改寫成
 *   MicroPython 版的正式中繼器(如原規劃，利於 list/dict)。
 *
 * 匯流排走 Serial2 (GPIO16=RX2, GPIO17=TX2)；USB Serial 留給除錯。
 * 接線見 terminal_arduino.ino 的註解 (注意 UNO 5V TX 要分壓後才進 ESP32 RX)。
 */

#define BUS_RX 16
#define BUS_TX 17
#define BUS_BAUD 9600
#define POLL_TIMEOUT_MS  200
#define POLL_INTERVAL_MS 1000

// 要輪詢的下層設備「本地位址」清單 (Stage 0 只有一台 = 4)
int children[] = {4};
const int N_CHILDREN = sizeof(children) / sizeof(children[0]);

void setup() {
  Serial.begin(115200);
  Serial2.begin(BUS_BAUD, SERIAL_8N1, BUS_RX, BUS_TX);
  delay(300);
  Serial.println("[master] up");
}

// 對某位址發一次請求，等 \r 收完整一筆。逾時回空字串。
String pollOne(int addr) {
  while (Serial2.available()) Serial2.read();   // 清掉殘留

  Serial2.print("REQ|");
  Serial2.print(addr);
  Serial2.print('\r');

  String buf = "";
  unsigned long start = millis();
  while (millis() - start < POLL_TIMEOUT_MS) {
    while (Serial2.available()) {
      char c = Serial2.read();
      if (c == '\r') { return buf; }   // \r = 完整一筆，收工
      if (c == '\n') { continue; }     // \n = 分段未完，繼續累積(Stage 0 用不到)
      buf += c;
      start = millis();                // 有在收就續命
    }
  }
  return "";   // 逾時
}

void loop() {
  for (int i = 0; i < N_CHILDREN; i++) {
    int addr = children[i];
    String data = pollOne(addr);

    if (data.length() > 0) {
      // 正式中繼器下一步要做：cache[addr] = data，
      // 並把 data 前綴自己的本地位址 → 向上拼成完整路徑 (如 1-4)
      Serial.print("[master] addr ");
      Serial.print(addr);
      Serial.print(" -> ");
      Serial.println(data);            // 例：4|{"t":"TMP","v":25.3,"u":"C","ok":1}
    } else {
      Serial.print("[master] addr ");
      Serial.print(addr);
      Serial.println(" -> TIMEOUT (stale)");
    }
  }
  delay(POLL_INTERVAL_MS);
}
