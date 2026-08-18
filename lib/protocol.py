"""
RS485_C 共用協定庫 (Python 版) — 模組 C
====================================================================
與 lib/protocol.h / lib/protocol.cpp 行為必須一致；以 lib/test_vectors.json 為準。

可在兩種環境執行：
  - CPython  → 模組 D（PC 上位機）
  - MicroPython → 模組 B（中繼器）
  （本檔避免使用 CPython 專屬語法/模組，維持 MicroPython 相容。）

範圍（見 子專案/C_協定庫.md，經定版確認為「只做框架 + 位址」）：
  1. \\n / \\r byte 串流框架的編碼 / 解碼
  2. <完整位址>|<單行json> 的組裝 / 解析
  3. 位址字串操作（前綴、切段、驗證）
不含 REQ| / CFG| 等請求指令字串（那留在韌體 A / 上位機 D）。

CRC 目前停用，但已預留接口：<位址>|<json>*<crc16>
  啟用時：設 CRC_ENABLED=True、實作 crc16()，build_payload / parse_payload
  會自動附加 / 解析 *<hex>。**啟用只需動這一份庫（與 protocol.cpp）。**

契約來源：docs/02_通訊協定.md、docs/03_定址與指撥開關.md、docs/06_設計決策與待確認.md
"""

PROTOCOL_VERSION = 1

# ---- 框架常數 ----
SEP = "|"          # 位址與 json 的分隔（0x7C）
CONT = "\n"        # 0x0A 「這筆完整、後面還有」（批次未完）
END = "\r"         # 0x0D 「這批到此結束」（最後一筆）
CRC_MARK = "*"     # 0x2A CRC 分隔（保留，尚未啟用）

CONT_B = 0x0A
END_B = 0x0D

# 位址每段合法範圍：1..63（0 保留給廣播 / 未設定，見 docs/06 A1）
ADDR_MIN = 1
ADDR_MAX = 63

# CRC 尚未啟用。啟用步驟見檔頭說明。
CRC_ENABLED = False


# ====================================================================
# CRC（保留接口）
# ====================================================================
def crc16(data):
    """CRC-16 保留接口。CRC_ENABLED=True 時才需實作（兩語言演算法需一致）。
    目前未使用。"""
    raise NotImplementedError("CRC reserved; not enabled (see docs/06 B3)")


def _crc_hex(v):
    return "{:04X}".format(v & 0xFFFF)


# ====================================================================
# Payload 記錄層： <位址>|<json>[*<crc>]
# ====================================================================
def build_payload(addr_path, json_str, crc=None):
    """組出 <位址>|<json>（不含結尾框架分隔符 \\n / \\r）。
    CRC 停用時 crc 應為 None。"""
    s = addr_path + SEP + json_str
    if CRC_ENABLED and crc is not None:
        s = s + CRC_MARK + _crc_hex(crc)
    return s


def parse_payload(s):
    """解析 <位址>|<json>[*<crc>] → (addr, json, crc)。
    - 以「第一個 '|'」切出位址與其餘。
    - CRC 停用時一律回 (addr, json, None)，字串中的 '*' 視為 json 內容。
    找不到 '|' 會丟 ValueError。"""
    i = s.find(SEP)
    if i < 0:
        raise ValueError("payload missing '|': " + repr(s))
    addr = s[:i]
    rest = s[i + 1:]
    crc = None
    if CRC_ENABLED:
        j = rest.rfind(CRC_MARK)
        if j >= 0:
            crc = int(rest[j + 1:], 16)
            rest = rest[:j]
    return (addr, rest, crc)


# ====================================================================
# 框架編碼
# ====================================================================
def encode(addr_path, json_str, final=True, crc=None):
    """組出「完整一筆」框架 bytes：payload + 結尾分隔符。
      final=True  → 結尾 \\r（這批最後一筆 / 單筆）
      final=False → 結尾 \\n（批次未完，後面還有）
    """
    payload = build_payload(addr_path, json_str, crc)
    term = END if final else CONT
    return (payload + term).encode("utf-8")


def encode_batch(items):
    """把多筆一次編碼成一個 bytes。
    items: 可迭代的 (addr_path, json_str)。最後一筆用 \\r，其餘用 \\n。
    空 items 回 b''。（中繼器一次回一批孩子時用。）"""
    items = list(items)
    out = b""
    n = len(items)
    for idx in range(n):
        out += encode(items[idx][0], items[idx][1], idx == n - 1)
    return out


# ====================================================================
# 框架解碼：逐 byte 餵入，遇 \\n / \\r 吐一筆記錄
# ====================================================================
class FrameDecoder:
    """逐 byte 餵入的框架解碼器（per-record emit，見定版決策）。
    每遇到一個分隔符就吐出「一筆」記錄，並附 final 旗標：
        \\n → (payload, False)   批次未完
        \\r → (payload, True)    批次結束
    與 C++ rs485c::FrameDecoder 行為一致。

    用法：
        dec = FrameDecoder()
        rec = dec.push(b)        # b: int 0..255 或長度1的 str/bytes
        if rec is not None:
            payload, final = rec
    """

    def __init__(self, max_len=512):
        self._buf = bytearray()
        self._max = max_len
        self.overflow = False   # 目前這筆是否曾溢位（best-effort 診斷）

    def reset(self):
        self._buf = bytearray()
        self.overflow = False

    def push(self, b):
        """餵一個 byte。收齊一筆回 (payload_str, final)；否則回 None。
        溢位的記錄會被丟棄並回 None（下一個分隔符後自動重新同步）。"""
        if isinstance(b, str):
            b = ord(b)
        elif isinstance(b, (bytes, bytearray)):
            b = b[0]

        if b == CONT_B or b == END_B:
            if self.overflow:
                # 這筆因超長而不可信 → 丟棄，重新同步
                self._buf = bytearray()
                self.overflow = False
                return None
            payload = bytes(self._buf).decode("utf-8")
            final = (b == END_B)
            self._buf = bytearray()
            return (payload, final)

        # 一般資料 byte
        if self.overflow:
            return None          # 溢位後持續吞到下一個分隔符
        if len(self._buf) >= self._max:
            self.overflow = True
            return None
        self._buf.append(b)
        return None


# ====================================================================
# 位址字串操作
# ====================================================================
def addr_prefix(local, child_path):
    """中繼器前綴：把孩子路徑接在自己的本地位址後面。
      addr_prefix(32, "4")    -> "32-4"
      addr_prefix(1, "32-4")  -> "1-32-4"
    child_path 為空（"" 或 None）時只回 str(local)。"""
    if not child_path:
        return str(local)
    return str(local) + "-" + child_path


def addr_split(path):
    """'1-32-4' -> [1, 32, 4]。不做範圍檢查；非數字段會丟 ValueError。"""
    return [int(seg) for seg in path.split("-")]


def addr_valid(path):
    """位址路徑是否合法：非空、每段為十進位整數且 1..63、無空段、無前後 '-'。
    0 不合法（保留給廣播，見 docs/06 A1）。"""
    if not path:
        return False
    for seg in path.split("-"):
        if not seg:
            return False
        for ch in seg:
            if ch < "0" or ch > "9":
                return False
        v = int(seg)
        if v < ADDR_MIN or v > ADDR_MAX:
            return False
    return True


def addr_local(path):
    """本地位址（路徑最後一段）。無效回 -1。"""
    if not path:
        return -1
    i = path.rfind("-")
    seg = path[i + 1:] if i >= 0 else path
    try:
        return int(seg)
    except ValueError:
        return -1


def addr_parent(path):
    """父路徑（去掉最後一段）。單段（頂層）回 ""。"""
    i = path.rfind("-")
    if i < 0:
        return ""
    return path[:i]
