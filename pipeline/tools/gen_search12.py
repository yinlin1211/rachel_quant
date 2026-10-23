# -*- coding: utf-8 -*-
"""第 12 轮：把「下跌」从二值条件变成连续变量。

第 11 轮：下跌股的反转是上涨股的 1.5~2.5 倍，覆盖 32%，两段样本一致。
本轮问：
  Z_*  按「跌幅 / 自身波动」分档（per-stock z），反转强度是否随跌幅单调增强？
  W_*  把跌幅当权重并入因子（而不是二值筛选），能否在不缩池子的前提下提高 IC？
  K_*  跌幅本身作为独立信号的符号与强度

用法：python3 gen_search12.py [out.txt]
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r12.txt'

R1 = "sub(div(close,delay(close,1)),1)"
TRAD = "gt(rank(ts_mean(amount,20)),0.5)"
BASE = "neg(sub(div(low,ts_mean(low,5)),1))"
Z = "div(%s, add(ts_std(%s,20),0.00001))" % (R1, R1)

out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


# ---- Z：按标准化跌幅分档（z 越小 = 跌得越深）----
BUCKETS = [(-99, -2.0), (-2.0, -1.0), (-1.0, -0.5), (-0.5, 0.0),
           (0.0, 0.5), (0.5, 1.0), (1.0, 2.0), (2.0, 99)]
for lo, hi in BUCKETS:
    parts = []
    if lo > -99:
        parts.append("gt(%s,%.1f)" % (Z, lo))
    if hi < 99:
        parts.append("lt(%s,%.1f)" % (Z, hi))
    cond = parts[0] if len(parts) == 1 else "mul(%s, %s)" % (parts[0], parts[1])
    tag = "Z_%s_%s" % ('m' if lo < 0 else 'p', str(abs(lo)).replace('.', '') if lo > -99 else 'inf')
    push(tag, "div(%s, %s)" % (BASE, cond))

# 可交易池内的 Z 分档
for lo, hi in BUCKETS:
    parts = ["gt(rank(ts_mean(amount,20)),0.5)"]
    if lo > -99:
        parts.append("gt(%s,%.1f)" % (Z, lo))
    if hi < 99:
        parts.append("lt(%s,%.1f)" % (Z, hi))
    cond = parts[0]
    for p in parts[1:]:
        cond = "mul(%s, %s)" % (cond, p)
    tag = "ZT_%s_%s" % ('m' if lo < 0 else 'p', str(abs(lo)).replace('.', '') if lo > -99 else 'inf')
    push(tag, "div(%s, %s)" % (BASE, cond))

# ---- W：跌幅作为连续权重 ----
DOWN = "max(0, neg(%s))" % Z          # 只在下跌时为正
DOWNR = "max(0, rank(neg(%s)))" % R1  # 截面分位版
push("W_ADD1", "div(mul(%s, add(1, %s)), %s)" % (BASE, DOWN, TRAD))
push("W_ADD05", "div(mul(%s, add(1, mul(0.5, %s))), %s)" % (BASE, DOWN, TRAD))
push("W_ADD2", "div(mul(%s, add(1, mul(2, %s))), %s)" % (BASE, DOWN, TRAD))
push("W_RANK", "div(mul(rank(%s), add(1, %s)), %s)" % (BASE, DOWNR, TRAD))
push("W_SQRT", "div(mul(%s, add(1, sqrt(%s))), %s)" % (BASE, DOWN, TRAD))
push("W_ONLYDOWN", "div(mul(%s, %s), %s)" % (BASE, DOWN, TRAD))

# ---- K：跌幅本身 ----
push("K_DOWN", "div(neg(%s), %s)" % (Z, TRAD))
push("K_DOWN_RANK", "div(rank(neg(%s)), %s)" % (Z, TRAD))
push("K_DOWN_R1", "div(neg(%s), %s)" % (R1, TRAD))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("m%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("m%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
