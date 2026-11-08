#!/usr/bin/env python3
# 逐笔比较 bt5 与 sim_live 的成交价（同一批订单）
# 用法: compare_ab.py <bt5_orders.csv> <sim_fills.csv> <out.csv>
import csv, sys, statistics
from collections import defaultdict

ORD = sys.argv[1]
FILL = sys.argv[2]
OUT = sys.argv[3]

symbols = [l.strip() for l in open("/opt/bar_data/Meta/tkr.txt") if l.strip()]
sym2c = {s: i for i, s in enumerate(symbols)}

def hhmmssmmm_to_ms(t):
    h, rem = divmod(t, 10000000)
    m, rem = divmod(rem, 100000)
    s, ms = divmod(rem, 1000)
    return ((h * 60 + m) * 60 + s) * 1000 + ms

orders = []
for line in open(ORD):
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    p = line.split(",")
    if len(p) < 5:
        continue
    sym, tm, side, qty, lp = p[0], int(p[1]), p[2], int(p[3]), int(p[4])
    if sym not in sym2c:
        continue
    orders.append({"sym": sym, "c": sym2c[sym], "ms": hhmmssmmm_to_ms(tm),
                   "side": side, "qty": qty, "lp": lp / 10000.0})

fills = defaultdict(list)
for r in csv.DictReader(open(FILL)):
    c = int(r["cidx"]); side = r["side"]
    fills[(c, side)].append((int(r["time_ms"]), int(r["qty"]), float(r["price"])))
for k in fills:
    fills[k].sort()

orders.sort(key=lambda o: (o["c"], o["side"], o["ms"]))
ptr = defaultdict(int)
rows = []
for o in orders:
    key = (o["c"], o["side"])
    fl = fills.get(key, [])
    i = ptr[key]
    # 跳过早于下单时间的成交
    while i < len(fl) and fl[i][0] < o["ms"]:
        i += 1
    got_q = 0
    got_v = 0.0
    first_t = None
    last_t = None
    limit_t = o["ms"] + 10 * 60000
    while i < len(fl) and got_q < o["qty"]:
        t, q, px = fl[i]
        if t > limit_t:
            break
        got_q += q
        got_v += q * px
        if first_t is None:
            first_t = t
        last_t = t
        i += 1
    ptr[key] = i
    if got_q <= 0:
        continue
    vwap = got_v / got_q
    bp = (vwap - o["lp"]) / o["lp"] * 1e4
    rows.append({"sym": o["sym"], "side": o["side"], "order_qty": o["qty"],
                 "filled_qty": got_q, "fill_ratio": got_q / o["qty"],
                 "bt5_px": o["lp"], "sim_vwap": round(vwap, 6),
                 "bp": round(bp, 4), "abs_bp": round(abs(bp), 4),
                 "first_fill_ms": first_t, "last_fill_ms": last_t})

with open(OUT, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else
                       ["sym","side","order_qty","filled_qty","fill_ratio","bt5_px","sim_vwap","bp","abs_bp","first_fill_ms","last_fill_ms"])
    w.writeheader()
    w.writerows(rows)

b = [r["abs_bp"] for r in rows]
b.sort()
def pct(p):
    return b[min(len(b) - 1, int(len(b) * p))] if b else float("nan")
print("bt5 orders=%d, matched=%d (%.1f%%)" % (len(orders), len(rows), 100.0 * len(rows) / max(1, len(orders))))
if b:
    print("abs bp: median=%.2f p75=%.2f p90=%.2f p95=%.2f max=%.2f" %
          (statistics.median(b), pct(0.75), pct(0.90), pct(0.95), b[-1]))
    print("within 10bp: %d/%d = %.1f%%" % (sum(1 for x in b if x <= 10), len(b), 100.0 * sum(1 for x in b if x <= 10) / len(b)))
    print("within 5bp : %.1f%%" % (100.0 * sum(1 for x in b if x <= 5) / len(b)))
    print("signed bp: mean=%.2f" % statistics.mean([r["bp"] for r in rows]))
print("avg fill ratio: %.2f%%" % (100.0 * statistics.mean([r["fill_ratio"] for r in rows]) if rows else 0))
print("-> %s" % OUT)
