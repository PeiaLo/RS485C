// AUTO-GENERATED from test_vectors.json by test_protocol.py — DO NOT EDIT.
#ifndef RS485C_TEST_VECTORS_H
#define RS485C_TEST_VECTORS_H

struct KV3 { const char* a; const char* b; const char* c; };
struct PrefixV { int local; const char* child; const char* expect; };
struct SplitV { const char* path; const unsigned char* segs; int nsegs; };
struct BoolV { const char* path; bool expect; };
struct IntV { const char* path; int expect; };
struct EncV { const char* addr; const char* json; bool final; const char* expect; };
struct DecRec { const char* payload; bool final; };
struct DecV { const char* stream; const DecRec* recs; int nrecs; };

static const KV3 BUILD_PAYLOAD_VECS[] = {
  {"4", "{\"t\":\"TMP\",\"v\":26.7,\"u\":\"C\",\"ok\":1}", "4|{\"t\":\"TMP\",\"v\":26.7,\"u\":\"C\",\"ok\":1}"},
  {"1-32-4", "{\"v\":1}", "1-32-4|{\"v\":1}"},
  {"4", "{}", "4|{}"},
};
static const int BUILD_PAYLOAD_N = 3;

static const KV3 PARSE_PAYLOAD_VECS[] = {
  {"1-32-4|{\"t\":\"TMP\",\"v\":26.7}", "1-32-4", "{\"t\":\"TMP\",\"v\":26.7}"},
  {"4|{}", "4", "{}"},
  {"7|{\"a\":\"b|c\"}", "7", "{\"a\":\"b|c\"}"},
};
static const int PARSE_PAYLOAD_N = 3;

static const EncV ENCODE_VECS[] = {
  {"4", "{\"v\":1}", true, "4|{\"v\":1}\r"},
  {"32-4", "{\"v\":2}", false, "32-4|{\"v\":2}\n"},
  {"1-32-4", "{\"t\":\"TMP\",\"v\":26.7,\"u\":\"C\",\"ok\":1}", true, "1-32-4|{\"t\":\"TMP\",\"v\":26.7,\"u\":\"C\",\"ok\":1}\r"},
};
static const int ENCODE_N = 3;

static const DecRec DECODE_0_RECS[] = {
  {"4|{\"v\":1}", true},
};
static const DecRec DECODE_1_RECS[] = {
  {"1|{\"v\":1}", false},
  {"2|{\"v\":2}", false},
  {"3|{\"v\":3}", true},
};
static const DecRec DECODE_2_RECS[] = {
  {"1-32-4|{\"t\":\"TMP\",\"v\":26.7}", true},
};
static const DecV DECODE_VECS[] = {
  {"4|{\"v\":1}\r", DECODE_0_RECS, 1},
  {"1|{\"v\":1}\n2|{\"v\":2}\n3|{\"v\":3}\r", DECODE_1_RECS, 3},
  {"1-32-4|{\"t\":\"TMP\",\"v\":26.7}\r", DECODE_2_RECS, 1},
};
static const int DECODE_N = 3;

static const PrefixV ADDR_PREFIX_VECS[] = {
  {32, "4", "32-4"},
  {1, "32-4", "1-32-4"},
  {5, "", "5"},
};
static const int ADDR_PREFIX_N = 3;

static const unsigned char ADDR_SPLIT_0_SEGS[] = {1, 32, 4};
static const unsigned char ADDR_SPLIT_1_SEGS[] = {4};
static const unsigned char ADDR_SPLIT_2_SEGS[] = {1, 2, 3, 4, 5};
static const SplitV ADDR_SPLIT_VECS[] = {
  {"1-32-4", ADDR_SPLIT_0_SEGS, 3},
  {"4", ADDR_SPLIT_1_SEGS, 1},
  {"1-2-3-4-5", ADDR_SPLIT_2_SEGS, 5},
};
static const int ADDR_SPLIT_N = 3;

static const BoolV ADDR_VALID_VECS[] = {
  {"1-32-4", true},
  {"4", true},
  {"63", true},
  {"1", true},
  {"0", false},
  {"64", false},
  {"1-0-4", false},
  {"1-64-4", false},
  {"1--4", false},
  {"-4", false},
  {"4-", false},
  {"", false},
  {"1-32-x", false},
};
static const int ADDR_VALID_N = 13;

static const IntV ADDR_LOCAL_VECS[] = {
  {"1-32-4", 4},
  {"7", 7},
  {"1-63", 63},
  {"", -1},
};
static const int ADDR_LOCAL_N = 4;

static const KV3 ADDR_PARENT_VECS[] = {
  {"1-32-4", "1-32", ""},
  {"32-4", "32", ""},
  {"4", "", ""},
};
static const int ADDR_PARENT_N = 3;

#endif // RS485C_TEST_VECTORS_H
