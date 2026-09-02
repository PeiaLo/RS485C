/*
 * RS485_C — Nucleo 主端 / 中繼器雛形（C++ / STM32duino），使用 lib/protocol.*
 * ====================================================================
 * 用途：快速驗證用的 master（與 esp32_master 同邏輯，但跑 Nucleo，序列埠可靠）。
 *   Bus  : Serial1 (USART1 = PA9 TX1 / PA10 RX1) → 接末端（或 MAX13487）
 *   對 PC: Serial   (Nucleo 板載 ST-Link VCP，可靠) → 看輪詢結果
 *
 * 板子：Nucleo-F401RE。Arduino Tools：
 *   Board = Nucleo-64 → Nucleo F401RE；USB support = **None**（讓 Serial = ST-Link VCP）；
 *   Upload = STM32CubeProgrammer (SWD)（板載 ST-Link，插上 USB 直接上傳）。
 *
 * ⚠️ 編譯前：sync_lib 把 protocol.h/.cpp 複製進本資料夾。
 * ⚠️ children 預設 {4,5}：只接 addr 4 末端時，addr 5 會 TIMEOUT（正常，示範逾時）。
 */
#include "protocol.h"

// ⚠️ Nucleo 變體預設沒實例化 Serial1(USART1) → 自己在 PA9(TX)/PA10(RX) 上建一個。
//    HardwareSerial(rx, tx)：第一個參數是 RX。
HardwareSerial SerialBus(PA10 /*RX1*/, PA9 /*TX1*/);
#define BUS       SerialBus
#define DBG       Serial
#define BUS_BAUD  115200
#define POLL_TIMEOUT_MS  200
#define POLL_INTERVAL_MS 1000

const uint8_t children[] = {4, 5};
const int N_CHILDREN = sizeof(children) / sizeof(children[0]);
char decBuf[128];

void setup() {
  DBG.begin(115200);
  BUS.begin(BUS_BAUD);
  delay(300);
  DBG.println("[master] up");
}

// 對某位址發 REQ、等 \r 收一筆。成功填 addr/json 回 true；逾時回 false。
bool pollOne(uint8_t addr, char* addrOut, int addrSize, char* jsonOut, int jsonSize) {
  while (BUS.available()) BUS.read();               // 清殘留
  BUS.print("REQ|"); BUS.print(addr); BUS.print('\r');

  rs485c::FrameDecoder dec(decBuf, sizeof(decBuf));
  unsigned long start = millis();
  while (millis() - start < POLL_TIMEOUT_MS) {      // 溢位安全
    while (BUS.available()) {
      uint8_t b = BUS.read();
      rs485c::FrameDecoder::Result r = dec.push(b);
      if (r == rs485c::FrameDecoder::FRAME_RECORD) {
        long crc;
        // RS485 半雙工會收到自己送的 REQ 回音、以及線上雜訊 → 只收「位址合法」的記錄，
        // 其餘跳過繼續讀（回音 "REQ|4" 的位址是 "REQ"，addr_valid=false 會被濾掉）。
        if (rs485c::parse_payload(dec.payload(), addrOut, addrSize, jsonOut, jsonSize, &crc)
            && rs485c::addr_valid(addrOut)) {
          return true;
        }
        // 無效 → 不 return，繼續讀下一筆（FrameDecoder 已自動重置）
      }
      start = millis();                             // 有在收就續命
    }
  }
  return false;                                     // 逾時 → stale
}

void loop() {
  for (int i = 0; i < N_CHILDREN; i++) {
    char addr[16], json[96];
    // 注意：STM32duino 的 Serial 沒有 printf，用 print 串接（不要用 DBG.printf）
    DBG.print("[master] addr "); DBG.print(children[i]); DBG.print(" -> ");
    if (pollOne(children[i], addr, sizeof(addr), json, sizeof(json))) {
      DBG.print(addr); DBG.print('|'); DBG.println(json);
    } else {
      DBG.println("TIMEOUT (stale)");
    }
  }
  delay(POLL_INTERVAL_MS);
}
