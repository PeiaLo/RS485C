/*
 * RS485_C 共用協定庫 (C++ 版) — 模組 C
 * ====================================================================
 * 與 lib/protocol.py 行為必須一致；以 lib/test_vectors.json 為準。
 *
 * 目標平台：Arduino UNO (AVR) 與 ESP32。
 *   → AVR 安全：不使用 String / STL / 動態記憶體（heap）。
 *   → 所有輸出都寫進「呼叫端提供的緩衝」，回傳長度或成功旗標。
 *
 * 範圍（定版確認為「只做框架 + 位址」）：
 *   1. \n / \r byte 串流框架的編碼 / 解碼
 *   2. <完整位址>|<單行json> 的組裝 / 解析
 *   3. 位址字串操作（前綴、切段、驗證）
 * 不含 REQ| / CFG| 請求指令（留在韌體 / 上位機）。
 *
 * CRC 停用，接口已預留：<位址>|<json>*<crc16>
 *   啟用時：設 CRC_ENABLED=true、實作 crc16()，build/parse 自動處理 *<hex>。
 *   啟用只需動這一份庫（與 protocol.py）。
 *
 * 契約來源：docs/02_通訊協定.md、docs/03_定址與指撥開關.md、docs/06。
 */
#ifndef RS485C_PROTOCOL_H
#define RS485C_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

namespace rs485c {

static const int PROTOCOL_VERSION = 1;

// ---- 框架常數 ----
static const char SEP      = '|';    // 位址與 json 的分隔
static const char CONT     = '\n';   // 0x0A 批次未完
static const char END      = '\r';   // 0x0D 批次結束
static const char CRC_MARK = '*';    // 0x2A CRC 分隔（保留）

// 位址每段合法範圍 1..63（0 保留給廣播，見 docs/06 A1）
static const uint8_t ADDR_MIN = 1;
static const uint8_t ADDR_MAX = 63;

// CRC 尚未啟用。啟用步驟見檔頭。
static const bool CRC_ENABLED = false;


// ====================================================================
// CRC（保留接口）—— 目前回 0、不被呼叫（CRC_ENABLED=false）
// ====================================================================
uint16_t crc16(const uint8_t* data, int len);


// ====================================================================
// Payload 記錄層： <位址>|<json>
// ====================================================================

// 組出 <位址>|<json> 到 out（含結尾 '\0'）。
// 回傳字串長度（不含 '\0'）；out_size 不足回 -1。
int build_payload(const char* addr_path, const char* json_str,
                  char* out, int out_size);

// 解析 <位址>|<json>[*<crc>]。
//   addr_out / json_out：呼叫端提供的緩衝（會補 '\0'）。
//   crc_out：可為 NULL；CRC 停用時填 -1。
// 回傳 true 成功；找不到 '|' 或緩衝不足回 false。
bool parse_payload(const char* s,
                   char* addr_out, int addr_size,
                   char* json_out, int json_size,
                   long* crc_out);


// ====================================================================
// 框架編碼
// ====================================================================

// 組出「完整一筆」框架到 out：payload + (final ? '\r' : '\n')，含 '\0'。
// 回傳寫入的 byte 數（不含 '\0'）；out_size 不足回 -1。
int encode(const char* addr_path, const char* json_str, bool final,
           char* out, int out_size);


// ====================================================================
// 框架解碼：逐 byte 餵入，遇 \n / \r 吐一筆（per-record emit）
// ====================================================================
class FrameDecoder {
public:
  enum Result {
    FRAME_NONE     = 0,   // 尚未收齊一筆
    FRAME_RECORD   = 1,   // 收到一筆：payload()/length()/final() 有效
    FRAME_OVERFLOW = 2    // 該筆超長被丟棄，已自動重新同步
  };

  // buf / cap 由呼叫端提供（AVR 免 heap）。cap 需含結尾 '\0' 的空間。
  FrameDecoder(char* buf, int cap);

  // 餵一個 byte。回傳見 Result。
  Result push(uint8_t b);

  const char* payload() const { return _buf; }   // FRAME_RECORD 後有效，null-terminated
  int         length()  const { return _len; }
  bool        final()   const { return _final; }
  void        reset();

private:
  char* _buf;
  int   _cap;
  int   _len;
  bool  _final;
  bool  _overflow;
  bool  _completed;   // 上一筆已交付；下個 byte 開始新的一筆
};


// ====================================================================
// 位址字串操作
// ====================================================================

// local + '-' + child_path → out。child_path 為 NULL 或 "" 時只放 local。
// 回傳長度；out_size 不足回 -1。
int addr_prefix(uint8_t local, const char* child_path, char* out, int out_size);

// '1-32-4' → out[]={1,32,4}。回傳段數；空段/非數字/超過 max_segs 回 -1。
// （不做 1..63 範圍檢查，與 Python addr_split 一致；範圍檢查用 addr_valid。）
int addr_split(const char* path, uint8_t* out, int max_segs);

// 每段整數 1..63、無空段、無前後 '-'。0 不合法。
bool addr_valid(const char* path);

// 本地位址（最後一段）。無效回 -1。
int addr_local(const char* path);

// 父路徑（去最後一段）到 out。單段回 ""（空字串）。
// 回傳長度；out_size 不足回 -1。
int addr_parent(const char* path, char* out, int out_size);

} // namespace rs485c

#endif // RS485C_PROTOCOL_H
