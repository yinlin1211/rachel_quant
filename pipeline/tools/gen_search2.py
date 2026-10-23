# -*- coding: utf-8 -*-
"""第 2 轮：组合、条件化、微观结构。

假设：
  C_*  两个弱相关信号的秩相加，比任一单信号更强（弱信号也能提供增量）
  K_*  反转在高波动/高流动性子集内更强；用 div(x, mask) 把子集外置为 NaN，
       IC 只在子集内统计（覆盖率会下降，必须如实报）
  M_*  分钟内的 close 相对 vwap 的位置（最后成交 vs 均价）是独立的微观结构信号

用法：python3 gen_search2.py [out.txt]
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r2.txt'

R1 = "sub(div(close,delay(close,1)),1)"


def dev(w):
    return "sub(div(close,ts_mean(close,%d)),1)" % w


def lowdev(w):
    return "sub(div(low,ts_mean(low,%d)),1)" % w


def vwapdev(w):
    return "sub(div(close,ts_mean(vwap,%d)),1)" % w


def ret(w):
    return "sub(div(close,delay(close,%d)),1)" % w


out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


# ---------- 基础信号（均已确认方向为负 IC，取 neg 后为正）----------
BASE = {
    'dev5':  "neg(%s)" % dev(5),
    'dev8':  "neg(%s)" % dev(8),
    'dev10': "neg(%s)" % dev(10),
    'low5':  "neg(%s)" % lowdev(5),
    'low8':  "neg(%s)" % lowdev(8),
    'ret1':  "neg(%s)" % ret(1),
    'dec5':  "neg(decay(%s,5))" % R1,
    'dec3':  "neg(decay(%s,3))" % R1,
    'vdev5': "neg(%s)" % vwapdev(5),
    'pos':   "neg(div(sub(close,low),add(sub(high,low),0.001)))",
}

# ---------- C：秩组合 ----------
names = list(BASE)
for i, a in enumerate(names):
    for b in names[i + 1:]:
        ra, rb = "rank(%s)" % BASE[a], "rank(%s)" % BASE[b]
        push("C_ADD", "add(%s, %s)" % (ra, rb))
        push("C_SUB", "sub(%s, %s)" % (ra, rb))
        push("C_MUL", "mul(%s, %s)" % (ra, rb))

# ---------- K：条件化（子集内计算 IC） ----------
CONDS = {
    'hi_vol50': "gt(rank(ts_std(%s,20)),0.5)" % R1,
    'hi_vol80': "gt(rank(ts_std(%s,20)),0.8)" % R1,
    'hi_amt50': "gt(rank(ts_mean(amount,20)),0.5)",
    'hi_amt80': "gt(rank(ts_mean(amount,20)),0.8)",
    'hi_volu50': "gt(rank(ts_mean(volume,20)),0.5)",
    'hi_volu80': "gt(rank(ts_mean(volume,20)),0.8)",
    'hi_range50': "gt(rank(div(sub(high,low),close)),0.5)",
    'big_move50': "gt(rank(abs(%s)),0.5)" % ret(1),
    'big_move80': "gt(rank(abs(%s)),0.8)" % ret(1),
}
for bn in ['dev5', 'dev8', 'low5', 'low8', 'dec5', 'ret1']:
    for cn, cond in CONDS.items():
        push("K_" + cn, "div(%s, %s)" % (BASE[bn], cond))

# ---------- M：微观结构 ----------
intra = "sub(div(close,vwap),1)"
push("M_INTRA", "neg(%s)" % intra)
push("M_INTRA_RANK", "rank(neg(%s))" % intra)
for w in [3, 5, 10, 20, 60]:
    push("M_INTRA_Z", "neg(div(%s, add(ts_std(%s,%d),0.0001)))" % (intra, intra, w))
    push("M_BARPOS", "neg(div(sub(close,low),add(sub(high,low),0.001)))")
    push("M_RANGE", "neg(div(sub(close,ts_max(high,%d)), add(ts_std(%s,%d),0.0001)))" % (w, R1, w))
    push("M_LOW", "neg(div(sub(low,ts_min(low,%d)), add(ts_std(%s,%d),0.0001)))" % (w, R1, w))
    push("M_CLV", "neg(div(sub(close,ts_mean(vwap,%d)), add(ts_std(%s,%d),0.0001)))" % (w, R1, w))
    # 最后 N 分钟相对更长期均线的偏离（多尺度）
    push("M_MULTI", "neg(div(sub(%s,%s), add(ts_std(%s,%d),0.0001)))" % (dev(3), dev(w), R1, w))

# ---------- 与量能的交互 ----------
for w in [3, 5, 8, 10, 20]:
    vr = "div(ts_mean(volume,%d),ts_mean(volume,60))" % w
    push("X_VOL", "mul(rank(%s), rank(%s))" % (BASE['dev%d' % w] if ('dev%d' % w) in BASE else "neg(%s)" % dev(w), vr))
    push("X_VOL_NEG", "mul(rank(%s), sub(2,rank(%s)))" % (BASE['dev%d' % w] if ('dev%d' % w) in BASE else "neg(%s)" % dev(w), vr))
    amr = "div(ts_mean(amount,%d),ts_mean(amount,60))" % w
    push("X_AMT", "mul(rank(%s), rank(%s))" % ("neg(%s)" % dev(w), amr))
    push("X_AMT_NEG", "mul(rank(%s), sub(2,rank(%s)))" % ("neg(%s)" % dev(w), amr))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("r%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("r%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
