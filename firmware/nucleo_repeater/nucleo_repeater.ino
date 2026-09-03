/*
 * RS485_C — Nucleo 中繼器（C++ / STM32duino），使用 lib/protocol.*
 * ====================================================================
 * 第 1 層中繼器（本地位址 1）。對下層是 master（輪詢+cache+前綴），
 * 對上層(PC) 直接用板載 VCP（免收發器）。
 *
 *   下層 bus: USART1 (PA9 TX1 / PA10 RX1) + MAX13487 → 末端
 *   上層(PC): Serial (板載 ST-Link VCP, COM8) → 免 MAX
 *
 * 快取：hold-last-value + age（逾時保留舊值、回報時附 age_ms）。
 * 前綴：末端報 "4" → 中繼器(本地1) 前綴成 "1-4"。
 *
 * 板子：Nucleo F401RE；USB support=None；Upload=SWD。
 * ⚠️ 編譯前 sync_lib 複製 protocol.h/.cpp 進本資料夾。
 *
 * PC 測試：python rs485_probe.py COM8 4     → 應收到 1-4|{...,"age":N}
 *          python rs485_probe.py COM8 ALL   → 回全部孩子
 */
#include "protocol.h"

HardwareSerial SerialDown(PA10 /*RX1*/, PA9 /*TX1*/);   // 下層 bus
#define DOWN SerialDown
#define UP   Serial                                     // 上層 = VCP → PC
#define BAUD 115200
#define POLL_TIMEOUT_MS 200
#define MY_ADDR 1                                        // 本中繼器在上層的本地位址

const uint8_t children[] = {4};                          // 下層末端的本地位址（可加 ,5 等）
const int N = sizeof(children) / sizeof(children[0]);

struct Slot { char path[24]; char json[72]; unsigned long ts; bool fresh; bool valid; };
Slot cache[N];
char decBuf[128];

void setup() {
  UP.begin(115200);
  DOWN.begin(BAUD);
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < N; i++) { cache[i].valid = false; cache[i].fresh = false; }
  delay(300);
  UP.print("[repeater] up, addr="); UP.println(MY_ADDR);
}

// 輪詢一個孩子：REQ → 收 \r → 前綴自己位址 → 存 cache。逾時保留舊值(hold-last)。
void pollChild(int i) {
  uint8_t addr = children[i];
  while (DOWN.available()) DOWN.read();
  DOWN.print("REQ|"); DOWN.print(addr); DOWN.print('\r');

  rs485c::FrameDecoder dec(decBuf, sizeof(decBuf));
  unsigned long start = millis();
  while (millis() - start < POLL_TIMEOUT_MS) {
    while (DOWN.available()) {
      uint8_t b = DOWN.read();
      if (dec.push(b) == rs485c::FrameDecoder::FRAME_RECORD) {
        char caddr[16], cjson[72]; long crc;
        // 只收位址合法的（濾掉 RS485 半雙工回音/雜訊）
        if (rs485c::parse_payload(dec.payload(), caddr, sizeof(caddr), cjson, sizeof(cjson), &crc)
            && rs485c::addr_valid(caddr)) {
          char path[24];
          rs485c::addr_prefix(MY_ADDR, caddr, path, sizeof(path));   // "4" → "1-4"
          strncpy(cache[i].path, path, sizeof(cache[i].path) - 1); cache[i].path[sizeof(cache[i].path) - 1] = 0;
          strncpy(cache[i].json, cjson, sizeof(cache[i].json) - 1); cache[i].json[sizeof(cache[i].json) - 1] = 0;
          cache[i].ts = millis(); cache[i].fresh = true; cache[i].valid = true;
          return;
        }
      }
      start = millis();
    }
  }
  if (cache[i].valid) cache[i].fresh = false;   // 逾時：保留舊值、標 stale
}

// 把某孩子的 cache 回給 PC（json 尾插 age_ms）
void serveSlot(int i) {
  if (!cache[i].valid) return;
  unsigned long age = millis() - cache[i].ts;
  UP.print(cache[i].path); UP.print('|');
  int L = strlen(cache[i].json);
  if (L > 0 && cache[i].json[L - 1] == '}') {
    for (int k = 0; k < L - 1; k++) UP.print(cache[i].json[k]);
    UP.print(",\"age\":"); UP.print(age); UP.print('}');
  } else {
    UP.print(cache[i].json);
  }
  UP.print('\r');
}

// 上層(PC) 要資料：REQ|<localaddr> 或 REQ|ALL → 撈 cache 回上去
void serveUpstream() {
  static char rb[40]; static int rl = 0;
  while (UP.available()) {
    char c = UP.read();
    if (c == '\r' || c == '\n') {
      rb[rl] = 0;
      if (strncmp(rb, "REQ|", 4) == 0) {
        const char* arg = rb + 4;
        if (strcmp(arg, "ALL") == 0) { for (int i = 0; i < N; i++) serveSlot(i); }
        else { int a = atoi(arg); for (int i = 0; i < N; i++) if (children[i] == a) serveSlot(i); }
      }
      rl = 0;
    } else if (rl < (int)sizeof(rb) - 1) {
      rb[rl++] = c;
    }
  }
}

void loop() {
  static bool led = false; static unsigned long lt = 0;
  if (millis() - lt > 250) { lt = millis(); led = !led; digitalWrite(LED_BUILTIN, led); }

  for (int i = 0; i < N; i++) {
    pollChild(i);       // 輪詢下層、更新 cache
    serveUpstream();    // 上層優先：每問完一個就看 PC 有沒有要
  }
}
