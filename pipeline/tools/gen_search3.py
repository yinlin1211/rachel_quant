# -*- coding: utf-8 -*-
"""第 3 轮：条件强度的阈值曲线 + 叠加条件 + 子集内组合。

上一轮结论：反转信号在高成交量股票上 IC 从 0.036 → 0.052，且三档单调。
本轮要回答：
  T_*  阈值越严 IC 越高吗？拐点在哪？覆盖率掉到多少就不值得？
  S_*  两个条件叠加（高量 × 高波动）是否比单条件更强？
  P_*  在子集内做组合，能否突破 0.055？
  A_*  用成交额（amount）替代成交量做条件，稳定性是否更好？

用法：python3 gen_search3.py [out.txt]
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r3.txt'

R1 = "sub(div(close,delay(close,1)),1)"


def lowd(w):
    return "neg(sub(div(low,ts_mean(low,%d)),1))" % w


def dev(w):
    return "neg(sub(div(close,ts_mean(close,%d)),1))" % w


def dec(w):
    return "neg(decay(%s,%d))" % (R1, w)


BASES = {
    'low3': lowd(3), 'low5': lowd(5), 'low8': lowd(8), 'low10': lowd(10),
    'dev5': dev(5), 'dev8': dev(8), 'dec3': dec(3), 'dec5': dec(5),
    'ret1': "neg(%s)" % R1,
}

VOLU = "rank(ts_mean(volume,20))"
AMT = "rank(ts_mean(amount,20))"
VOLA = "rank(ts_std(%s,20))" % R1
RANGE = "rank(div(sub(high,low),close))"

out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


# ---- T：单条件阈值曲线 ----
for cname, cond in [('volu', VOLU), ('amt', AMT), ('vola', VOLA)]:
    for t in [0.5, 0.6, 0.7, 0.8, 0.9]:
        for bn in ['low3', 'low5', 'low8', 'dev5', 'dec3', 'dec5', 'ret1']:
            push("T_%s%d" % (cname, int(t * 10)), "div(%s, gt(%s,%.1f))" % (BASES[bn], cond, t))

# ---- S：叠加条件 ----
STACKS = {
    'volu_vola': (VOLU, VOLA),
    'volu_amt': (VOLU, AMT),
    'amt_vola': (AMT, VOLA),
    'volu_range': (VOLU, RANGE),
}
for sn, (c1, c2) in STACKS.items():
    for t in [0.5, 0.7, 0.8]:
        cond = "mul(gt(%s,%.1f), gt(%s,%.1f))" % (c1, t, c2, t)
        for bn in ['low5', 'low8', 'dev5', 'dec5']:
            push("S_%s_%d" % (sn, int(t * 10)), "div(%s, %s)" % (BASES[bn], cond))

# ---- P：子集内组合 ----
hi = "gt(%s,0.8)" % VOLU
pairs = [('low5', 'dec3'), ('low5', 'dev5'), ('low5', 'ret1'),
         ('dev5', 'dec3'), ('low8', 'dec5'), ('low5', 'low8')]
for a, b in pairs:
    comp = "add(rank(%s), rank(%s))" % (BASES[a], BASES[b])
    push("P_ADD", "div(%s, %s)" % (comp, hi))
    push("P_MUL", "div(mul(rank(%s), rank(%s)), %s)" % (BASES[a], BASES[b], hi))
    push("P_SUB", "div(sub(rank(%s), rank(%s)), %s)" % (BASES[a], BASES[b], hi))

# ---- A：成交额条件（20 / 60 分钟窗口） ----
for w in [20, 60]:
    cond = "gt(rank(ts_mean(amount,%d)),0.8)" % w
    for bn in ['low5', 'low8', 'dev5', 'dec3']:
        push("A_AMT%d" % w, "div(%s, %s)" % (BASES[bn], cond))

# ---- 子集内再按波动率加权（软条件） ----
for bn in ['low5', 'dev5', 'dec5']:
    push("W_VOLA", "div(mul(rank(%s), rank(ts_std(%s,20))), %s)" % (BASES[bn], R1, hi))
    push("W_IVOLA", "div(mul(rank(%s), sub(2,rank(ts_std(%s,20)))), %s)" % (BASES[bn], R1, hi))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("q%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("q%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
