#!/usr/bin/env python3
"""
RS485_C 協定庫 — Python 測試執行器（模組 C）
====================================================================
做兩件事：
  1. 讀 test_vectors.json，驗證 protocol.py 全數通過（DoD）。
  2. 由同一份 JSON 產生 test_vectors.h，供 C++ 測試（test_protocol.cpp）使用，
     確保兩語言跑的是同一組向量（唯一真相來源 = test_vectors.json）。

用法：
    python test_protocol.py            # 跑 Python 測試 + 產生 test_vectors.h
    python test_protocol.py --gen-only # 只產生 header，不跑測試
全過回傳 exit code 0；有失敗回 1。（可在 MicroPython 上用需自備 json/檔案 IO；
本執行器以 CPython 為主。）
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import protocol as P   # noqa: E402


def load_vectors():
    with open(os.path.join(HERE, "test_vectors.json"), "r", encoding="utf-8") as f:
        return json.load(f)


# ---------------------------------------------------------------- 測試
class Runner:
    def __init__(self):
        self.passed = 0
        self.failed = 0

    def check(self, name, got, want):
        if got == want:
            self.passed += 1
        else:
            self.failed += 1
            print("  FAIL {}\n       got : {!r}\n       want: {!r}".format(name, got, want))

    def run(self, v):
        for i, c in enumerate(v["build_payload"]):
            self.check("build_payload[{}]".format(i),
                       P.build_payload(c["addr"], c["json"]), c["expect"])

        for i, c in enumerate(v["parse_payload"]):
            addr, js, crc = P.parse_payload(c["input"])
            self.check("parse_payload[{}].addr".format(i), addr, c["addr"])
            self.check("parse_payload[{}].json".format(i), js, c["json"])
            self.check("parse_payload[{}].crc".format(i), crc, None)

        for i, c in enumerate(v["encode"]):
            got = P.encode(c["addr"], c["json"], c["final"])
            self.check("encode[{}]".format(i), got, c["expect"].encode("utf-8"))

        for i, c in enumerate(v["decode"]):
            dec = P.FrameDecoder()
            recs = []
            for ch in c["stream"]:
                r = dec.push(ord(ch))
                if r is not None:
                    recs.append({"payload": r[0], "final": r[1]})
            self.check("decode[{}].count".format(i), len(recs), len(c["records"]))
            for k, want in enumerate(c["records"]):
                if k < len(recs):
                    self.check("decode[{}].rec[{}]".format(i, k), recs[k], want)

        for i, c in enumerate(v["addr_prefix"]):
            self.check("addr_prefix[{}]".format(i),
                       P.addr_prefix(c["local"], c["child"]), c["expect"])

        for i, c in enumerate(v["addr_split"]):
            self.check("addr_split[{}]".format(i),
                       P.addr_split(c["path"]), c["expect"])

        for i, c in enumerate(v["addr_valid"]):
            self.check("addr_valid[{}] ({!r})".format(i, c["path"]),
                       P.addr_valid(c["path"]), c["expect"])

        for i, c in enumerate(v["addr_local"]):
            self.check("addr_local[{}]".format(i),
                       P.addr_local(c["path"]), c["expect"])

        for i, c in enumerate(v["addr_parent"]):
            self.check("addr_parent[{}]".format(i),
                       P.addr_parent(c["path"]), c["expect"])


# ------------------------------------------------- 產生 C++ test_vectors.h
def cesc(s):
    """把字串轉成 C 字串字面值內容（不含外層引號）。"""
    out = []
    for ch in s:
        o = ord(ch)
        if ch == '\\':
            out.append('\\\\')
        elif ch == '"':
            out.append('\\"')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\r':
            out.append('\\r')
        elif ch == '\t':
            out.append('\\t')
        elif 32 <= o < 127:
            out.append(ch)
        else:
            out.append('\\x{:02x}'.format(o))
    return ''.join(out)


def q(s):
    return '"' + cesc(s) + '"'


def gen_header(v):
    L = []
    L.append("// AUTO-GENERATED from test_vectors.json by test_protocol.py — DO NOT EDIT.")
    L.append("#ifndef RS485C_TEST_VECTORS_H")
    L.append("#define RS485C_TEST_VECTORS_H")
    L.append("")
    L.append("struct KV3 { const char* a; const char* b; const char* c; };")
    L.append("struct PrefixV { int local; const char* child; const char* expect; };")
    L.append("struct SplitV { const char* path; const unsigned char* segs; int nsegs; };")
    L.append("struct BoolV { const char* path; bool expect; };")
    L.append("struct IntV { const char* path; int expect; };")
    L.append("struct EncV { const char* addr; const char* json; bool final; const char* expect; };")
    L.append("struct DecRec { const char* payload; bool final; };")
    L.append("struct DecV { const char* stream; const DecRec* recs; int nrecs; };")
    L.append("")

    # build_payload: KV3 = (addr, json, expect)
    L.append("static const KV3 BUILD_PAYLOAD_VECS[] = {")
    for c in v["build_payload"]:
        L.append("  {" + q(c["addr"]) + ", " + q(c["json"]) + ", " + q(c["expect"]) + "},")
    L.append("};")
    L.append("static const int BUILD_PAYLOAD_N = %d;" % len(v["build_payload"]))
    L.append("")

    # parse_payload: KV3 = (input, addr, json)
    L.append("static const KV3 PARSE_PAYLOAD_VECS[] = {")
    for c in v["parse_payload"]:
        L.append("  {" + q(c["input"]) + ", " + q(c["addr"]) + ", " + q(c["json"]) + "},")
    L.append("};")
    L.append("static const int PARSE_PAYLOAD_N = %d;" % len(v["parse_payload"]))
    L.append("")

    # encode
    L.append("static const EncV ENCODE_VECS[] = {")
    for c in v["encode"]:
        L.append("  {" + q(c["addr"]) + ", " + q(c["json"]) + ", " +
                 ("true" if c["final"] else "false") + ", " + q(c["expect"]) + "},")
    L.append("};")
    L.append("static const int ENCODE_N = %d;" % len(v["encode"]))
    L.append("")

    # decode (nested)
    for i, c in enumerate(v["decode"]):
        L.append("static const DecRec DECODE_%d_RECS[] = {" % i)
        for r in c["records"]:
            L.append("  {" + q(r["payload"]) + ", " + ("true" if r["final"] else "false") + "},")
        L.append("};")
    L.append("static const DecV DECODE_VECS[] = {")
    for i, c in enumerate(v["decode"]):
        L.append("  {" + q(c["stream"]) + ", DECODE_%d_RECS, %d}," % (i, len(c["records"])))
    L.append("};")
    L.append("static const int DECODE_N = %d;" % len(v["decode"]))
    L.append("")

    # addr_prefix
    L.append("static const PrefixV ADDR_PREFIX_VECS[] = {")
    for c in v["addr_prefix"]:
        L.append("  {%d, %s, %s}," % (c["local"], q(c["child"]), q(c["expect"])))
    L.append("};")
    L.append("static const int ADDR_PREFIX_N = %d;" % len(v["addr_prefix"]))
    L.append("")

    # addr_split (nested seg arrays)
    for i, c in enumerate(v["addr_split"]):
        segs = ", ".join(str(x) for x in c["expect"])
        L.append("static const unsigned char ADDR_SPLIT_%d_SEGS[] = {%s};" % (i, segs))
    L.append("static const SplitV ADDR_SPLIT_VECS[] = {")
    for i, c in enumerate(v["addr_split"]):
        L.append("  {%s, ADDR_SPLIT_%d_SEGS, %d}," % (q(c["path"]), i, len(c["expect"])))
    L.append("};")
    L.append("static const int ADDR_SPLIT_N = %d;" % len(v["addr_split"]))
    L.append("")

    # addr_valid
    L.append("static const BoolV ADDR_VALID_VECS[] = {")
    for c in v["addr_valid"]:
        L.append("  {%s, %s}," % (q(c["path"]), "true" if c["expect"] else "false"))
    L.append("};")
    L.append("static const int ADDR_VALID_N = %d;" % len(v["addr_valid"]))
    L.append("")

    # addr_local
    L.append("static const IntV ADDR_LOCAL_VECS[] = {")
    for c in v["addr_local"]:
        L.append("  {%s, %d}," % (q(c["path"]), c["expect"]))
    L.append("};")
    L.append("static const int ADDR_LOCAL_N = %d;" % len(v["addr_local"]))
    L.append("")

    # addr_parent (KV: path, expect) -> reuse KV3 with c unused
    L.append("static const KV3 ADDR_PARENT_VECS[] = {")
    for c in v["addr_parent"]:
        L.append("  {%s, %s, \"\"}," % (q(c["path"]), q(c["expect"])))
    L.append("};")
    L.append("static const int ADDR_PARENT_N = %d;" % len(v["addr_parent"]))
    L.append("")

    L.append("#endif // RS485C_TEST_VECTORS_H")
    L.append("")
    return "\n".join(L)


def main():
    gen_only = "--gen-only" in sys.argv
    v = load_vectors()

    # 產生 C++ header
    hp = os.path.join(HERE, "test_vectors.h")
    with open(hp, "w", encoding="utf-8", newline="\n") as f:
        f.write(gen_header(v))
    print("generated {}".format(hp))

    if gen_only:
        return 0

    r = Runner()
    r.run(v)
    total = r.passed + r.failed
    print("\nPython: {}/{} passed, {} failed".format(r.passed, total, r.failed))
    return 0 if r.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
