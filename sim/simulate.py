#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RS485_C 網路模擬試跑
====================================================================
用剛定版的 lib/protocol.py，實跑一棵多層樹的取樣資料流。
**每一跳都真的走 encode / FrameDecoder / parse_payload / addr_prefix**，
所以這不只是畫圖，是把「共用契約」端到端跑一遍：

  末端(報本地位址) → 中繼器(前綴自己位址、批次) → …… → PC(解出完整路徑)

也示範：批次框架(\\n…\\r)、離線設備逾時被跳過(stale)、逐跳前綴拼出全路徑。

用法： python sim/simulate.py
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "lib"))
import protocol as P   # noqa: E402


# ---------------------------------------------------------------- 節點模型
class Terminal:
    """末端傳感器：只知道自己的本地位址，被輪詢就回一筆 <local>|<json>\\r。"""
    def __init__(self, addr, type_, value, online=True):
        self.addr = addr
        self.type = type_
        self.value = value
        self.online = online
        self.children = []

    def emit(self):
        if not self.online:
            return b""                       # 離線 → 送不出東西 → 上層會逾時
        js = '{"t":"%s","v":%s,"ok":1}' % (self.type, self.value)
        return P.encode(str(self.addr), js, final=True)   # 單筆，結尾 \r


class Repeater:
    """中繼器：對下層是 master(輪詢+cache)，對上層是 slave(被問才回)。"""
    def __init__(self, addr, children):
        self.addr = addr
        self.children = children
        self.cache = {}                      # child_local -> (records|None, state)

    def poll(self):
        """輪詢每個孩子：收 bytes → 解框 → 前綴自己的位址 → 存 cache。逾時標 stale。"""
        self.cache = {}
        for ch in self.children:
            raw = ch.emit()
            dec = P.FrameDecoder()
            recs = []
            for b in raw:                    # 逐 byte 餵入，per-record emit
                r = dec.push(b)
                if r is not None:
                    recs.append(r)
            if not recs:
                self.cache[ch.addr] = (None, "stale")     # 逾時 → 跳過
                continue
            prefixed = []
            for payload, final in recs:
                caddr, cjson, _ = P.parse_payload(payload)
                prefixed.append((P.addr_prefix(self.addr, caddr), cjson))  # 逐跳前綴
            self.cache[ch.addr] = (prefixed, "fresh")

    def emit(self):
        """被上層輪詢 → 先 poll 下層，再把 cache 內所有記錄組成一批 bytes 回上去。"""
        self.poll()
        records = []
        for ch in self.children:
            data, state = self.cache[ch.addr]
            if state == "fresh":
                records.extend(data)
        return P.encode_batch(records)       # 多筆：前面 \n、最後 \r


# ---------------------------------------------------------------- 建一棵樹
def build_tree():
    lvl2a = Repeater(32, [
        Terminal(4, "TMP", "26.7"),
        Terminal(5, "PRS", "101.3"),
        Terminal(7, "RH", "45.2", online=False),   # 故意離線 → 示範 stale
    ])
    lvl2b = Repeater(10, [
        Terminal(2, "DI", "1"),
        Terminal(3, "TMP", "24.1"),
    ])
    return Repeater(1, [lvl2a, lvl2b])             # 第 1 層中繼器，本地位址 1


def print_topology(node, indent="", parent_path=""):
    path = parent_path + ("-" if parent_path else "") + str(node.addr)
    if isinstance(node, Terminal):
        mark = "" if node.online else "  ← 離線 (會逾時/ stale)"
        print("%s• 末端 [%s] %s=%s   本地位址 %d%s"
              % (indent, path, node.type, node.value, node.addr, mark))
    else:
        print("%s▣ 中繼器  本地位址 %d" % (indent, node.addr))
        for ch in node.children:
            print_topology(ch, indent + "    ", path)


def main():
    print("=" * 64)
    print("RS485_C 模擬試跑（走真實 lib/protocol.py）")
    print("=" * 64)
    root = build_tree()

    print("\n拓樸：\n")
    print("PC")
    print_topology(root, "  ")

    print("\n" + "-" * 64)
    print("PC 對第 1 層中繼器發 REQ|ALL，收回一批 bytes：")
    print("-" * 64)
    raw = root.emit()                          # 走完整條鏈：末端→中繼→中繼→PC
    print("\n原始 bytes（\\n=批次未完, \\r=批次結束）：")
    print("  " + repr(raw))

    print("\nPC 端用 FrameDecoder 解出的完整記錄：\n")
    dec = P.FrameDecoder()
    n = 0
    for b in raw:
        r = dec.push(b)
        if r is not None:
            payload, final = r
            addr, js, _ = P.parse_payload(payload)
            n += 1
            tag = "  (批次最後一筆)" if final else ""
            ok = "✓" if P.addr_valid(addr) else "✗位址不合法"
            print("  [%d] 完整路徑 %-10s %s  %s%s"
                  % (n, addr, ok, js, tag))

    print("\n說明：")
    print("  - 離線的末端 1-32-7 沒有出現 → 被中繼器逾時跳過、標 stale（符合契約）。")
    print("  - 每條完整路徑都是逐跳前綴拼出來的：末端只報 4，中繼器 32 前綴成 32-4，")
    print("    中繼器 1 再前綴成 1-32-4，PC 才看到全路徑。")
    print("  - 一批多筆用 \\n 分隔、最後一筆 \\r 收尾，PC 逐 byte 解出每一筆。")
    print("  - 全程 encode / FrameDecoder / parse_payload / addr_prefix 都是 lib 的實作。")


if __name__ == "__main__":
    main()
