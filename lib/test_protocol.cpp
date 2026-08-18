/*
 * RS485_C 協定庫 — C++ 測試執行器（host / g++，非 Arduino）
 * ====================================================================
 * 用 test_vectors.h（由 test_protocol.py 從 test_vectors.json 產生）驗證
 * protocol.cpp 與 protocol.py 行為一致。
 *
 * 編譯 & 執行（在有 g++ 的環境）：
 *     python test_protocol.py --gen-only      # 先產生 test_vectors.h
 *     g++ -std=c++11 -I. test_protocol.cpp protocol.cpp -o test_protocol
 *     ./test_protocol
 * 全過回傳 exit code 0；有失敗回 1。
 *
 * 註：本檔僅用於 PC 上驗證邏輯；AVR 端只需編譯 protocol.cpp。
 */
#include "protocol.h"
#include "test_vectors.h"
#include <cstdio>
#include <cstring>

using namespace rs485c;

static int g_pass = 0;
static int g_fail = 0;

static void ck_str(const char* name, const char* got, const char* want) {
  if (strcmp(got, want) == 0) { g_pass++; }
  else { g_fail++; printf("  FAIL %s\n       got : \"%s\"\n       want: \"%s\"\n", name, got, want); }
}

static void ck_int(const char* name, long got, long want) {
  if (got == want) { g_pass++; }
  else { g_fail++; printf("  FAIL %s  got=%ld want=%ld\n", name, got, want); }
}

static void ck_bool(const char* name, bool got, bool want) {
  if (got == want) { g_pass++; }
  else { g_fail++; printf("  FAIL %s  got=%d want=%d\n", name, (int)got, (int)want); }
}

int main() {
  char buf[256];
  char addr[64];
  char json[256];
  long crc;

  // build_payload
  for (int i = 0; i < BUILD_PAYLOAD_N; i++) {
    const KV3& c = BUILD_PAYLOAD_VECS[i];   // a=addr b=json c=expect
    int n = build_payload(c.a, c.b, buf, sizeof(buf));
    char name[48]; snprintf(name, sizeof(name), "build_payload[%d]", i);
    if (n < 0) { g_fail++; printf("  FAIL %s (buffer)\n", name); }
    else ck_str(name, buf, c.c);
  }

  // parse_payload
  for (int i = 0; i < PARSE_PAYLOAD_N; i++) {
    const KV3& c = PARSE_PAYLOAD_VECS[i];   // a=input b=addr c=json
    bool ok = parse_payload(c.a, addr, sizeof(addr), json, sizeof(json), &crc);
    char name[48];
    snprintf(name, sizeof(name), "parse_payload[%d].ok", i); ck_bool(name, ok, true);
    snprintf(name, sizeof(name), "parse_payload[%d].addr", i); ck_str(name, addr, c.b);
    snprintf(name, sizeof(name), "parse_payload[%d].json", i); ck_str(name, json, c.c);
    snprintf(name, sizeof(name), "parse_payload[%d].crc", i); ck_int(name, crc, -1);
  }

  // encode
  for (int i = 0; i < ENCODE_N; i++) {
    const EncV& c = ENCODE_VECS[i];
    int n = encode(c.addr, c.json, c.final, buf, sizeof(buf));
    char name[48]; snprintf(name, sizeof(name), "encode[%d]", i);
    if (n < 0) { g_fail++; printf("  FAIL %s (buffer)\n", name); }
    else ck_str(name, buf, c.expect);
  }

  // decode
  for (int i = 0; i < DECODE_N; i++) {
    const DecV& c = DECODE_VECS[i];
    char dbuf[256];
    FrameDecoder dec(dbuf, sizeof(dbuf));
    int got = 0;
    bool ok = true;
    for (const char* p = c.stream; *p; p++) {
      FrameDecoder::Result r = dec.push((uint8_t)*p);
      if (r == FrameDecoder::FRAME_RECORD) {
        char name[48];
        if (got < c.nrecs) {
          snprintf(name, sizeof(name), "decode[%d].rec[%d].payload", i, got);
          ck_str(name, dec.payload(), c.recs[got].payload);
          snprintf(name, sizeof(name), "decode[%d].rec[%d].final", i, got);
          ck_bool(name, dec.final(), c.recs[got].final);
        } else { ok = false; }
        got++;
      }
    }
    char name[48]; snprintf(name, sizeof(name), "decode[%d].count", i);
    ck_int(name, got, c.nrecs);
    (void)ok;
  }

  // addr_prefix
  for (int i = 0; i < ADDR_PREFIX_N; i++) {
    const PrefixV& c = ADDR_PREFIX_VECS[i];
    int n = addr_prefix((uint8_t)c.local, c.child, buf, sizeof(buf));
    char name[48]; snprintf(name, sizeof(name), "addr_prefix[%d]", i);
    if (n < 0) { g_fail++; printf("  FAIL %s (buffer)\n", name); }
    else ck_str(name, buf, c.expect);
  }

  // addr_split
  for (int i = 0; i < ADDR_SPLIT_N; i++) {
    const SplitV& c = ADDR_SPLIT_VECS[i];
    uint8_t out[16];
    int n = addr_split(c.path, out, 16);
    char name[48]; snprintf(name, sizeof(name), "addr_split[%d].count", i);
    ck_int(name, n, c.nsegs);
    if (n == c.nsegs) {
      for (int k = 0; k < n; k++) {
        snprintf(name, sizeof(name), "addr_split[%d].seg[%d]", i, k);
        ck_int(name, out[k], c.segs[k]);
      }
    }
  }

  // addr_valid
  for (int i = 0; i < ADDR_VALID_N; i++) {
    const BoolV& c = ADDR_VALID_VECS[i];
    char name[48]; snprintf(name, sizeof(name), "addr_valid[%d] (%s)", i, c.path);
    ck_bool(name, addr_valid(c.path), c.expect);
  }

  // addr_local
  for (int i = 0; i < ADDR_LOCAL_N; i++) {
    const IntV& c = ADDR_LOCAL_VECS[i];
    char name[48]; snprintf(name, sizeof(name), "addr_local[%d]", i);
    ck_int(name, addr_local(c.path), c.expect);
  }

  // addr_parent
  for (int i = 0; i < ADDR_PARENT_N; i++) {
    const KV3& c = ADDR_PARENT_VECS[i];   // a=path b=expect
    int n = addr_parent(c.a, buf, sizeof(buf));
    char name[48]; snprintf(name, sizeof(name), "addr_parent[%d]", i);
    if (n < 0) { g_fail++; printf("  FAIL %s (buffer)\n", name); }
    else ck_str(name, buf, c.b);
  }

  printf("\nC++: %d/%d passed, %d failed\n", g_pass, g_pass + g_fail, g_fail);
  return g_fail == 0 ? 0 : 1;
}
