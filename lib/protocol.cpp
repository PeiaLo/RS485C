/*
 * RS485_C 共用協定庫 (C++ 版實作) — 模組 C
 * 與 lib/protocol.py 行為一致；以 lib/test_vectors.json 為準。
 * AVR 安全：純 char 緩衝、無 String / STL / heap。
 */
#include "protocol.h"

namespace rs485c {

// -------- 內部小工具（不對外） --------
static int _slen(const char* s) {
  int n = 0;
  while (s && s[n]) n++;
  return n;
}

static int _index_of(const char* s, char c) {
  for (int i = 0; s && s[i]; i++)
    if (s[i] == c) return i;
  return -1;
}

static int _last_index_of(const char* s, char c) {
  int r = -1;
  for (int i = 0; s && s[i]; i++)
    if (s[i] == c) r = i;
  return r;
}

// 無號整數 → 十進位字串（避免依賴 sprintf/itoa，兩平台一致）。
static int _uint_to_dec(unsigned long v, char* out) {
  if (v == 0) { out[0] = '0'; out[1] = '\0'; return 1; }
  char tmp[12];
  int n = 0;
  while (v > 0) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
  for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
  out[n] = '\0';
  return n;
}


// ====================================================================
// CRC（保留）
// ====================================================================
uint16_t crc16(const uint8_t* data, int len) {
  (void)data; (void)len;
  return 0;   // TODO: 啟用 CRC 時實作（需與 protocol.py 一致）
}


// ====================================================================
// Payload 記錄層
// ====================================================================
int build_payload(const char* addr, const char* json, char* out, int out_size) {
  int la = _slen(addr);
  int lj = _slen(json);
  int need = la + 1 /*SEP*/ + lj;      // 不含 '\0'
  if (need + 1 > out_size) return -1;  // 需容 '\0'
  int p = 0;
  for (int i = 0; i < la; i++) out[p++] = addr[i];
  out[p++] = SEP;
  for (int i = 0; i < lj; i++) out[p++] = json[i];
  out[p] = '\0';
  // CRC_ENABLED=false → 不附加 *<crc>
  return p;
}

bool parse_payload(const char* s, char* addr_out, int addr_size,
                   char* json_out, int json_size, long* crc_out) {
  if (crc_out) *crc_out = -1;
  int i = _index_of(s, SEP);
  if (i < 0) return false;
  if (i + 1 > addr_size) return false;   // i 個字 + '\0'
  for (int k = 0; k < i; k++) addr_out[k] = s[k];
  addr_out[i] = '\0';

  const char* rest = s + i + 1;
  int lr = _slen(rest);
  // CRC 停用：整段當 json（字串中的 '*' 視為 json 內容）
  if (lr + 1 > json_size) return false;
  for (int k = 0; k < lr; k++) json_out[k] = rest[k];
  json_out[lr] = '\0';
  return true;
}


// ====================================================================
// 框架編碼
// ====================================================================
int encode(const char* addr, const char* json, bool final,
           char* out, int out_size) {
  int n = build_payload(addr, json, out, out_size);
  if (n < 0) return -1;
  if (n + 2 > out_size) return -1;       // 結尾符 + '\0'
  out[n] = final ? END : CONT;
  out[n + 1] = '\0';
  return n + 1;
}


// ====================================================================
// 框架解碼
// ====================================================================
FrameDecoder::FrameDecoder(char* buf, int cap)
  : _buf(buf), _cap(cap), _len(0), _final(false),
    _overflow(false), _completed(false) {
  if (_cap > 0) _buf[0] = '\0';
}

void FrameDecoder::reset() {
  _len = 0;
  _final = false;
  _overflow = false;
  _completed = false;
  if (_cap > 0) _buf[0] = '\0';
}

FrameDecoder::Result FrameDecoder::push(uint8_t b) {
  // 上一筆已交付 → 開始新的一筆（讓呼叫端有機會讀完 payload()）
  if (_completed) {
    _len = 0;
    _final = false;
    _overflow = false;
    _completed = false;
  }

  if (b == (uint8_t)CONT || b == (uint8_t)END) {
    if (_overflow) {
      // 該筆超長不可信 → 丟棄、重新同步
      _overflow = false;
      _len = 0;
      if (_cap > 0) _buf[0] = '\0';
      return FRAME_OVERFLOW;
    }
    _buf[_len] = '\0';
    _final = (b == (uint8_t)END);
    _completed = true;
    return FRAME_RECORD;
  }

  // 一般資料 byte
  if (_overflow) return FRAME_NONE;        // 溢位後持續吞到下一個分隔符
  if (_len >= _cap - 1) {                   // 需留 '\0'
    _overflow = true;
    return FRAME_NONE;
  }
  _buf[_len++] = (char)b;
  return FRAME_NONE;
}


// ====================================================================
// 位址字串操作
// ====================================================================
int addr_prefix(uint8_t local, const char* child, char* out, int out_size) {
  char num[4];
  int ln = _uint_to_dec((unsigned long)local, num);
  int lc = (child == 0) ? 0 : _slen(child);
  int need = ln + (lc > 0 ? 1 + lc : 0);   // 不含 '\0'
  if (need + 1 > out_size) return -1;
  int p = 0;
  for (int i = 0; i < ln; i++) out[p++] = num[i];
  if (lc > 0) {
    out[p++] = '-';
    for (int i = 0; i < lc; i++) out[p++] = child[i];
  }
  out[p] = '\0';
  return p;
}

int addr_split(const char* path, uint8_t* out, int max_segs) {
  if (!path || !*path) return -1;
  int count = 0;
  long v = 0;
  bool has = false;
  for (const char* p = path; ; p++) {
    char c = *p;
    if (c == '-' || c == '\0') {
      if (!has) return -1;                 // 空段
      if (count >= max_segs) return -1;
      out[count++] = (uint8_t)v;
      v = 0; has = false;
      if (c == '\0') break;
    } else if (c >= '0' && c <= '9') {
      v = v * 10 + (c - '0');
      has = true;
      if (v > 255) return -1;              // 防溢位（範圍檢查交給 addr_valid）
    } else {
      return -1;                            // 非數字
    }
  }
  return count;
}

bool addr_valid(const char* path) {
  if (!path || !*path) return false;
  int v = 0;
  bool has = false;
  for (const char* p = path; ; p++) {
    char c = *p;
    if (c == '-' || c == '\0') {
      if (!has) return false;              // 空段 / 前後 '-'
      if (v < ADDR_MIN || v > ADDR_MAX) return false;
      v = 0; has = false;
      if (c == '\0') break;
    } else if (c >= '0' && c <= '9') {
      v = v * 10 + (c - '0');
      has = true;
      if (v > 9999) return false;          // 防溢位
    } else {
      return false;
    }
  }
  return true;
}

int addr_local(const char* path) {
  if (!path || !*path) return -1;
  int i = _last_index_of(path, '-');
  const char* seg = (i >= 0) ? path + i + 1 : path;
  if (!*seg) return -1;
  int v = 0;
  for (const char* p = seg; *p; p++) {
    if (*p < '0' || *p > '9') return -1;
    v = v * 10 + (*p - '0');
  }
  return v;
}

int addr_parent(const char* path, char* out, int out_size) {
  int i = _last_index_of(path, '-');
  if (i < 0) {
    if (out_size < 1) return -1;
    out[0] = '\0';
    return 0;
  }
  if (i + 1 > out_size) return -1;
  for (int k = 0; k < i; k++) out[k] = path[k];
  out[i] = '\0';
  return i;
}

} // namespace rs485c
