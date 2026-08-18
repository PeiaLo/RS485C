# lib — 共用協定庫（模組 C）

`\n`/`\r` 框架 + `<位址>|<json>` 組裝解析 + 位址字串操作，**C++ 與 Python 兩份行為一致**，是所有模組（A 末端 / B 中繼器 / D 上位機）的共用契約。

> 契約定版：per-record emit 框架、純 char buffer 的 C++ API、只做「框架 + 位址」（不含 `REQ|`/`CFG|`）、CRC 停用但預留接口。決策見 [docs/06](../docs/06_設計決策與待確認.md)。

## 檔案

| 檔 | 給誰 | 說明 |
|----|------|------|
| `protocol.py` | 模組 B(MicroPython)、模組 D(CPython) | Python 實作，MicroPython 相容 |
| `protocol.h` / `protocol.cpp` | 模組 A(Arduino/AVR、ESP32) | C++ 實作，AVR 安全（無 String/STL/heap） |
| `test_vectors.json` | 兩邊 | **唯一真相來源**：同輸入 → 同輸出 |
| `test_protocol.py` | — | 跑 Python 測試 + 由 JSON 產生 `test_vectors.h` |
| `test_protocol.cpp` | — | host(g++) C++ 測試，用 `test_vectors.h` |

## 契約總表

### 框架常數
`SEP='|'(0x7C)`、`CONT='\n'(0x0A)` 批次未完、`END='\r'(0x0D)` 批次結束、`CRC_MARK='*'(0x2A)` 保留。位址每段 **1..63**（`0` 保留廣播）。

### API 對照（Python ↔ C++）

| 功能 | Python | C++ (`namespace rs485c`) |
|------|--------|--------------------------|
| 組 payload | `build_payload(addr, json) -> str` | `int build_payload(addr, json, out, out_size)` |
| 解析 payload | `parse_payload(s) -> (addr, json, crc)` | `bool parse_payload(s, addr_out, asz, json_out, jsz, crc_out*)` |
| 編碼一筆 | `encode(addr, json, final=True) -> bytes` | `int encode(addr, json, final, out, out_size)` |
| 編碼一批 | `encode_batch([(addr,json),...]) -> bytes` | （呼叫端迴圈 `encode`，最後一筆 `final=true`） |
| 解碼 | `FrameDecoder().push(b) -> (payload,final)\|None` | `FrameDecoder(buf,cap).push(b) -> Result` |
| 前綴 | `addr_prefix(local, child) -> str` | `int addr_prefix(local, child, out, out_size)` |
| 切段 | `addr_split(path) -> [int]` | `int addr_split(path, uint8_t* out, max)` |
| 驗證 | `addr_valid(path) -> bool` | `bool addr_valid(path)` |
| 本地位址 | `addr_local(path) -> int` | `int addr_local(path)`（無效 -1） |
| 父路徑 | `addr_parent(path) -> str` | `int addr_parent(path, out, out_size)` |

**解碼語意（per-record emit）**：每遇 `\n`/`\r` 吐「一筆」記錄，帶 `final` 旗標（`\r`→True）。
單筆回報 = 一筆 `\r`；中繼器一次回一批孩子 = 多筆 `\n` + 最後一筆 `\r`。

### CRC（保留，未啟用）
框架 `<位址>|<json>*<crc16>`。啟用步驟：兩份庫都設 `CRC_ENABLED=true`、實作 `crc16()`（演算法需一致），`build_payload`/`parse_payload` 會自動附加/解析 `*<hex>`。**只動這兩個檔。**

## 測試

Python（可直接跑，需 CPython）：

```bash
cd lib
python test_protocol.py
```

會驗證 `protocol.py` 全數通過，並產生 `test_vectors.h`。

C++（需 g++ 等 host 編譯器）：

```bash
cd lib
python test_protocol.py --gen-only
g++ -std=c++11 -I. test_protocol.cpp protocol.cpp -o test_protocol
./test_protocol
```

> DoD：兩邊跑同一份 `test_vectors.json` 全過。目前 Python 端 49/49 通過；C++ 端請在有編譯器的環境執行上列指令驗證。

## Arduino 用法示意

```cpp
#include "protocol.h"
using namespace rs485c;

char frame[128];
int n = encode("4", "{\"t\":\"TMP\",\"v\":26.7,\"ok\":1}", true, frame, sizeof(frame));
bus.write((const uint8_t*)frame, n);   // 送出（含結尾 \r）

// 接收：
char rxbuf[128];
FrameDecoder dec(rxbuf, sizeof(rxbuf));
while (bus.available()) {
  if (dec.push(bus.read()) == FrameDecoder::FRAME_RECORD) {
    // dec.payload() = "4|{...}", dec.final() = true/false
  }
}
```

## 下一步（不屬本次契約定版）
- 模組 A：把 `firmware/*/*.ino` 現有的 `\n`/`\r` 手寫邏輯改成 `#include "protocol.h"`，行為不變（C 子專案 DoD 第 2 條）。
