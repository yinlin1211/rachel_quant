#!/usr/bin/env python3
# 按 order_id 严格配对：bt5 订单 vs sim_live 成交
# 用法: compare_ab2.py <bt5_orders.csv> <sim_replay.csv> <sim_fills.csv> <out.csv>
import csv, sys, statistics
from collections import defaultdict

ORD, REP, FILL, OUT = sys.argv[1:5]

def hhmmssmmm_to_ms(t):
    h, rem = divmod(t, 10000000)
    m, rem = divmod(rem, 100000)
    s, ms = divmod(rem, 1000)
    return ((h * 60 + m) * 60 + s) * 1000 + ms

# sim 实际提交的订单
subs = {}
for r in csv.DictReader(open(REP)):
    key = (r["symbol"], r["side"], int(r["ms"]), int(r["qty"]))
    subs[key] = int(r["id"])

# sim 成交按 id 聚合
agg = defaultdict(lambda: [0, 0.0])
for r in csv.DictReader(open(FILL)):
    oid = int(r["order_id"])
    q = int(r["qty"]); px = float(r["price"])
    agg[oid][0] += q
    agg[oid][1] += q * px

rows = []
missing = 0
for line in open(ORD):
    line = line.strip()
    if not line or line.startswith("#"):
        continue
    p = line.split(",")
    if len(p) < 5:
        continue
    sym, tm, side, qty, lp = p[0], int(p[1]), p[2], int(p[3]), int(p[4])
    key = (sym, side, hhmmssmmm_to_ms(tm), qty)
    oid = subs.get(key)
    if oid is None:
        missing += 1
        continue
    got_q, got_v = agg.get(oid, [0, 0.0])
    if got_q <= 0:
        continue
    vwap = got_v / got_q
    bt5_px = lp / 10000.0
    bp = (vwap - bt5_px) / bt5_px * 1e4
    rows.append({"sym": sym, "side": side, "qty": qty, "filled_qty": got_q,
                 "ratio": round(got_q / qty, 4), "bt5_px": bt5_px,
                 "sim_vwap": round(vwap, 6), "bp": round(bp, 4),
                 "abs_bp": round(abs(bp), 4), "order_id": oid})

with open(OUT, "w", newline="") as f:
    if rows:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
    else:
        w = csv.DictWriter(f, fieldnames=["sym","side","qty","filled_qty","ratio","bt5_px","sim_vwap","bp","abs_bp","order_id"])
    w.writeheader(); w.writerows(rows)

b = sorted(r["abs_bp"] for r in rows)
def pct(p): return b[min(len(b)-1, int(len(b)*p))] if b else float("nan")
print("bt5 orders=%d, sim submitted=%d, not submitted=%d, filled=%d" %
      (sum(1 for _ in open(ORD)), len(subs), missing, len(rows)))
if b:
    print("abs bp: median=%.2f p75=%.2f p90=%.2f p95=%.2f p99=%.2f max=%.2f" %
          (statistics.median(b), pct(.75), pct(.90), pct(.95), pct(.99), b[-1]))
    print("<=10bp: %d/%d = %.1f%%   <=5bp: %.1f%%" %
          (sum(1 for x in b if x <= 10), len(b), 100.0*sum(1 for x in b if x <= 10)/len(b),
           100.0*sum(1 for x in b if x <= 5)/len(b)))
    print("signed bp mean=%.2f  avg fill ratio=%.2f%%" %
          (statistics.mean([r["bp"] for r in rows]), 100*statistics.mean([r["ratio"] for r in rows])))
print("-> %s" % OUT)
