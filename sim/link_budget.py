#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RS485_C 鏈路預算計算機（牆有多大）
====================================================================
回答：
  (2) 前端中繼器 ↔ PC 有線(RS485/MAX13487)實際可達最高頻率？
  (4) 這面牆內最多幾層？（受限於什麼）
  (5) 「先塞滿每條 bus、再加層」的策略含意與缺點。

模型前提（你的定案）：
  - 資料以 1 byte 為單位串流；8N1 → 每 byte = 10 bit。
  - 每條 bus 64 位址、留 1 備用 → 可用 63 台。
  - 收發器 MAX13487（AutoDirection，免 DE/RE）：**保證資料率上限 500 kbps**。
    （一般帶方向控制的 RS485 可到數 Mbps，但 AutoDirection 版本上限就是 500k。）
"""

BITS_PER_BYTE = 10           # 8N1
NODES = 63                   # 每條 bus 可用台數
REQ_BYTES = 8                # 例 "REQ|ALL\r"
REC_BYTES = 40               # 一筆 <完整路徑>|<單行json>，估 40 bytes
SLAVE_TURN_US = 200          # 每次交易的固定開銷：slave 反應時間 + 自動方向轉向死區

BAUDS = [9600, 38400, 115200, 250000, 500000]
MAX13487_MAX = 500000        # MAX13487 保證上限


def byte_time_us(baud):
    return BITS_PER_BYTE / baud * 1e6


def transaction_us(baud):
    """一次「請求+回覆」交易時間（含固定開銷）。"""
    return (REQ_BYTES + REC_BYTES) * byte_time_us(baud) + SLAVE_TURN_US


def bus_round_s(baud, nodes=NODES):
    """一條 bus 輪詢完 nodes 台一圈的時間。"""
    return nodes * transaction_us(baud) / 1e6


def stream_records_per_s(baud):
    """把『整包 cache』往上串流時，頂層鏈路每秒能吐幾筆記錄（純吞吐上限）。"""
    return (baud / BITS_PER_BYTE) / REC_BYTES


def sep():
    print("-" * 74)


def q2_link_frequency():
    print("=" * 74)
    print("(2) 前端中繼器 ↔ PC 有線鏈路：實際可達最高頻率")
    print("=" * 74)
    print("%-9s | %-10s | %-16s | %-14s | %s" %
          ("baud", "每byte", "一次交易(req+resp)", "交易/秒", "整包串流 記錄/秒"))
    sep()
    for b in BAUDS:
        cap = "  ← MAX13487 上限" if b == MAX13487_MAX else ""
        print("%-9d | %6.1f us | %13.2f ms | %10.1f /s | %8.0f 筆/s%s" %
              (b, byte_time_us(b), transaction_us(b) / 1000,
               1e6 / transaction_us(b), stream_records_per_s(b), cap))
    print("""
  重點：
  - MAX13487 硬上限 = 500 kbps（AutoDirection 版）。留裕度、含線材，
    實務設計點建議 250 kbps。想更快只能換帶方向控制的收發器或走模組 F。
  - 「最高頻率」看你要哪一種：
      · 單筆小交易（req+ack）：500k 下約 860 次/秒，115200 下約 230 次/秒。
      · 整包 cache 串流：500k 下約 1,250 筆/秒（每筆 40B），115200 下約 288 筆/秒。
  - 前端中繼器是 ESP32（硬體 UART，可飆 5Mbps），所以瓶頸是收發器不是 MCU。
    （末端若是 UNO 走 SoftwareSerial，那條葉端 bus 才會被卡在 ~38.4k 以下。）
""")


def q4_wall():
    print("=" * 74)
    print("(4) 牆有多大：最多幾層 / 受限於什麼")
    print("=" * 74)
    print("""
  兩道牆，先撞哪道看你怎麼用：

  牆A｜頂層頻寬（要『整包 cache』時的硬牆，限制總設備數）
  ---------------------------------------------------------------
  PC 從第 1 層拉整棵樹時，頂層鏈路要吐『全部末端』的記錄。
  上限 = 頂層鏈路 記錄/秒 × 你能接受的整包刷新秒數。""")
    print("    以 MAX13487 500 kbps（1,250 筆/秒）為例：")
    for R in (1, 3, 10):
        cap = int(stream_records_per_s(MAX13487_MAX) * R)
        # 這對應到「大約幾層滿樹」
        if cap < 63:
            layers = "不到 1 層"
        elif cap < 63**2:
            layers = "約 1 層滿 + 部分第 2 層"
        elif cap < 63**3:
            layers = "約 2 層滿附近"
        else:
            layers = "3 層以上"
        print("      整包刷新 %2ds → 最多約 %6s 台（%s）" %
              (R, "{:,}".format(cap), layers))
    print("""
    → 純有線 MAX13487、又要秒級整包刷新：總量天花板約 1,250～3,750 台，
      也就是『大約 2 層』就到頂。要再大 → 上模組 F（Ethernet/WiFi）換頂層鏈路。

  牆B｜端到端新鮮度延遲（限制層數）
  ---------------------------------------------------------------
  cache 樹讓各層解耦：葉端『自己的新鮮度』≈ 它那條 bus 一圈時間（見下）。
  但一個新數值要『冒到 PC』最壞要等每一層各輪一圈 → 端到端延遲 ≈ 層數 × 一圈。""")
    print("    每條 bus 滿 63 台時，一圈時間與『延遲預算內可疊幾層』：")
    print("    %-9s | %-12s | %s" % ("baud", "一圈(63台)", "延遲預算 2s / 5s 內可疊層數"))
    sep()
    for b in BAUDS:
        t = bus_round_s(b)
        d2 = int(2.0 / t) if t <= 2.0 else 0
        d5 = int(5.0 / t)
        print("    %-9d | %8.2f s   | %d 層 / %d 層" % (b, t, d2, d5))
    print("""
    → 若葉端 bus 塞滿 63 台，低 baud（9600）光一層就 >3s，層數牆很緊；
      高 baud（115200+）一圈才 0.1~0.3s，延遲上可疊 5~7 層——
      但這時通常先撞到牆A（頻寬），不是牆B。

  結論：這面牆的『層數』本身不是主限制；主限制是頂層頻寬（總量）。
        純有線 MAX13487 實務天花板 ≈ 2 層（數千台）。要更深更多 →
        (a) PC 改『要指定位址』而非每次要整包，或 (b) 頂層換 Ethernet/WiFi。
""")


def q5_fill_strategy():
    print("=" * 74)
    print("(5) 策略：先塞滿每條 bus、每層塞滿、再加層 —— 對嗎？缺點？")
    print("=" * 74)
    print("""
  對，就『最大化總設備數』而言邏輯正確：
    每條 bus 寬度被定址鎖死在 63（不能再寬），所以『塞滿寬度』是免費容量；
    要突破 63^當前層 只能靠加層。因此順序＝先填滿成完整 63 元樹、再加深，
    數學上就是 63^D，確實是最省層數拿到最多台的排法。

  但『塞好塞滿再加深』的缺點（都跟上面兩道牆有關）：
    1. 延遲最大化：滿 bus 的一圈最長（63×交易），端到端延遲 ≈ 層數×一圈，
       所以『又滿又深』對即時控制最不利。要快 → bus 別塞滿，反而更新鮮。
    2. 葉端新鮮度變差：同一條 bus 塞越多台，每台被輪到的間隔越久。
    3. 故障半徑(blast radius)大：一台中繼器掛掉 → 底下整棵子樹全失聯
       （越深越滿，單點故障波及越多台）。
    4. 頂層頻寬爆得快：塞滿又加層 → 總記錄數指數暴增 → 最先撞牆A。
    5. 佈線/調機/除錯：單條 bus 63 drop 反射與偏壓難搞、63 個指撥容易撥錯撞址、
       長路徑(1-32-4-…)難診斷。
    6. 易長歪：貪心塞滿可能一支獨深、其他空著 → 各分支延遲不均。

  折衷建議：
    - 要總量 → 你的『塞滿再加深』對，但認清代價在延遲與頂層頻寬。
    - 要即時控制 → 反過來：bus 別塞滿(例如每條 10~20 台)、樹儘量『矮而寬且平衡』，
      並讓 PC 多用『要指定位址』而非每次拉整包。
    - 不論哪種：頂層鏈路是總量瓶頸，一旦上千台就規劃模組 F。
""")


def main():
    q2_link_frequency()
    q4_wall()
    q5_fill_strategy()


if __name__ == "__main__":
    main()
