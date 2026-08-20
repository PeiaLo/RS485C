/*
 * RS485_C — ESP32 主端 / 中繼器雛形（Arduino/C++），使用 lib/protocol.*
 * ====================================================================
 * 角色：模組 B 的最小雛形。每秒 round-robin 輪詢下層、用 rs485c::FrameDecoder
 *       逐 byte 解框、rs485c::parse_payload 拆出 <位址>|<json>、印出；逾時標 stale。
 *
 *   Bus 走 Serial2 (GPIO16=RX2, GPIO17=TX2)；USB Serial 留給除錯。
 *
 * ⚠️ 編譯前：把 lib/protocol.h 與 lib/protocol.cpp 複製到「本 sketch 資料夾」。
 *    指令與接線見 firmware/STAGE0_兩台ESP32.md。
 *
 * children 預設 {4, 5}：
 *   - 只接 1 台 ESP32 末端(addr 4) → addr 5 會 TIMEOUT(stale)，正好示範逾時。
 *   - Stage 1 再接 UNO 末端(addr 5) 上真 RS485，就兩台都收得到。
 */
#include "protocol.h"

#define BUS_RX   16
#define BUS_TX   17
#define BUS_BAUD 115200
#define POLL_TIMEOUT_MS  200
#define POLL_INTERVAL_MS 1000

const uint8_t children[] = {4, 5};
const int N_CHILDREN = sizeof(children) / sizeof(children[0]);

char decBuf[128];   // FrameDecoder 用的緩衝（呼叫端提供，AVR 免 heap；ESP32 也照此慣例）

void setup() {
  Serial.begin(115200);
  Serial2.begin(BUS_BAUD, SERIAL_8N1, BUS_RX, BUS_TX);
  delay(300);
  Serial.println("[master] up");
}

// 對某位址發一次 REQ，等 \r 收完整一筆。成功填 addr/json 回 true；逾時回 false。
bool pollOne(uint8_t addr,
             char* addrOut, int addrSize,
             char* jsonOut, int jsonSize) {
  while (Serial2.available()) Serial2.read();     // 清殘留
  Serial2.print("REQ|");
  Serial2.print(addr);
  Serial2.print('\r');

  rs485c::FrameDecoder dec(decBuf, sizeof(decBuf));
  unsigned long start = millis();
  while (millis() - start < POLL_TIMEOUT_MS) {    // 溢位安全比較
    while (Serial2.available()) {
      uint8_t b = Serial2.read();
      rs485c::FrameDecoder::Result r = dec.push(b);
      if (r == rs485c::FrameDecoder::FRAME_RECORD) {
        long crc;
        return rs485c::parse_payload(dec.payload(),
                                     addrOut, addrSize, jsonOut, jsonSize, &crc);
      }
      start = millis();                           // 有在收就續命
    }
  }
  return false;   // 逾時 → stale
}

void loop() {
  for (int i = 0; i < N_CHILDREN; i++) {
    char addr[16], json[96];
    if (pollOne(children[i], addr, sizeof(addr), json, sizeof(json))) {
      // 正式中繼器下一步：cache[addr]=…，並前綴自己的本地位址向上拼路徑
      Serial.printf("[master] addr %d -> %s|%s\n", children[i], addr, json);
    } else {
      Serial.printf("[master] addr %d -> TIMEOUT (stale)\n", children[i]);
    }
  }
  delay(POLL_INTERVAL_MS);
}
