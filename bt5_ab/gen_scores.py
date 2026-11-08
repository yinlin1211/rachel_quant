#!/usr/bin/env python3
# 从分钟面板生成 bt5 信号：symbol,HHMMSSmmm,score
# score = 3 分钟收益 / 过去 20 分钟 1 分钟收益标准差（z 值）
import struct, sys

PANEL = sys.argv[1] if len(sys.argv) > 1 else "/home/intern2/rachel/pipeline/panels/20260105.panel"
OUT   = sys.argv[2] if len(sys.argv) > 2 else "/home/intern2/rachel/bt5_ab/scores_20260105.csv"
BASKET = sys.argv[3] if len(sys.argv) > 3 else "/home/intern2/rachel/bt5_demo/basket.csv"

ROW = 48
FMT = "<5i7f"

def slot_to_hhmmssmmm(slot):
    if slot < 120:
        minute = 570 + slot          # 09:30 + slot
    else:
        minute = 780 + (slot - 120)  # 13:00 + (slot-120)
    h, m = divmod(minute, 60)
    return h * 10000000 + m * 100000 + 0  # HHMMSSmmm, 秒=0

# tkr.txt: cidx -> symbol
symbols = []
with open("/opt/bar_data/Meta/tkr.txt") as f:
    for line in f:
        line = line.strip()
        if line:
            symbols.append(line)

basket = set()
with open(BASKET) as f:
    for i, line in enumerate(f):
        line = line.strip()
        if not line or (i == 0 and line.startswith("TKR")):
            continue
        basket.add(line.split(",")[0])

data = open(PANEL, "rb").read()
n = len(data) // ROW
assert len(data) % ROW == 0

# per-cidx minute arrays
N = len(symbols)
closes = [[0.0] * 240 for _ in range(N)]
has = [[False] * 240 for _ in range(N)]

for i in range(n):
    rec = struct.unpack_from(FMT, data, i * ROW)
    hhmm = rec[1] // 100          # 93000 -> 930
    cidx = rec[2]
    close = rec[8]
    # 注意：hhmm 是 HHMM 十进制（如 1000 = 10:00），不能直接减 930 算槽位。
    # 必须先折算成「距零点的分钟数」，否则跨小时后整体错位
    # （10:00 会被算成 slot=70，正确值是 30；14:00 之后更会越界）。
    h_, m_ = hhmm // 100, hhmm % 100
    mod = h_ * 60 + m_
    if 570 <= mod < 690:
        slot = mod - 570
    elif 780 <= mod < 900:
        slot = 120 + (mod - 780)
    else:
        continue
    if 0 <= cidx < N and 0 <= slot < 240 and close > 0:
        closes[cidx][slot] = close
        has[cidx][slot] = True

out = []
for c in range(N):
    sym = symbols[c]
    if sym not in basket:
        continue
    cs, hs = closes[c], has[c]
    for t in range(20, 240):
        if not (hs[t] and hs[t-3] and cs[t-3] > 0):
            continue
        rs = []
        for k in range(t-19, t+1):
            if hs[k] and hs[k-1] and cs[k-1] > 0:
                rs.append(cs[k] / cs[k-1] - 1.0)
        if len(rs) < 10:
            continue
        mean = sum(rs) / len(rs)
        var = sum((r - mean) ** 2 for r in rs) / len(rs)
        sd = var ** 0.5
        if sd <= 1e-9:
            continue
        r3 = cs[t] / cs[t-3] - 1.0
        z = r3 / sd
        if abs(z) > 10:
            continue
        out.append((sym, slot_to_hhmmssmmm(t), z))

with open(OUT, "w") as f:
    for sym, tm, z in out:
        f.write("%s,%d,%.6f\n" % (sym, tm, z))

print("panel rows=%d stocks=%d basket=%d signals=%d -> %s" % (n, N, len(basket), len(out), OUT))
