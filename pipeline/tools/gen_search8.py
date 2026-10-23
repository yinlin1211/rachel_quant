# -*- coding: utf-8 -*-
"""第 8 轮：在可交易池（成交额前 50%）里找**独立**信号，并做组合。

前情：所有 >0.05 的结果都被证明来自不活跃股票，可交易池内的上限约 0.039。
本轮不再切股票池，改为：
  A_*  逐个测试非反转类信号（量价相关、自相关、波动、振幅、位置、跳空）
  B_*  把它们与反转做线性组合（加 / 减 / 乘），看是否超过单反转
  C_*  两个非反转信号之间的组合

输出全部在成交额前 50% 的股票池内统计（mask 方式），覆盖率约 50%。
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r8.txt'

R1 = "sub(div(close,delay(close,1)),1)"
TRAD = "gt(rank(ts_mean(amount,20)),0.5)"
REV = "neg(sub(div(close,ts_mean(close,8)),1))"

out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


# ---- A：单信号 ----
SIG = {}
for w in [5, 10, 20, 60]:
    SIG['corr_cv%d' % w] = "ts_corr(close,volume,%d)" % w
    SIG['corr_lv%d' % w] = "ts_corr(low,volume,%d)" % w
    SIG['autocorr%d' % w] = "ts_corr(%s, delay(%s,1), %d)" % (R1, R1, w)
    SIG['vola%d' % w] = "ts_std(%s,%d)" % (R1, w)
    SIG['range%d' % w] = "ts_mean(div(sub(high,low),close),%d)" % w
    SIG['mom%d' % w] = "sub(div(close,delay(close,%d)),1)" % w
    SIG['distmax%d' % w] = "sub(div(close,ts_max(high,%d)),1)" % w
    SIG['distmin%d' % w] = "sub(div(close,ts_min(low,%d)),1)" % w
    SIG['argmax%d' % w] = "ts_argmax(close,%d)" % w
    SIG['argmin%d' % w] = "ts_argmin(close,%d)" % w
SIG['gap'] = "sub(div(open,delay(close,1)),1)"
SIG['intra'] = "sub(div(close,vwap),1)"
SIG['volratio'] = "div(ts_mean(volume,5),ts_mean(volume,60))"
SIG['amtratio'] = "div(ts_mean(amount,5),ts_mean(amount,60))"

for sn, s in SIG.items():
    push("A_" + sn, "div(%s, %s)" % (s, TRAD))
    push("A_neg_" + sn, "div(neg(%s), %s)" % (s, TRAD))

# ---- B：与反转组合 ----
COMBO = {k: v for k, v in SIG.items() if k.startswith(('corr_', 'autocorr', 'range', 'vola', 'distmax', 'distmin', 'gap', 'volratio'))}
for sn, s in COMBO.items():
    push("B_ADD_" + sn, "div(add(rank(%s), rank(%s)), %s)" % (REV, s, TRAD))
    push("B_SUB_" + sn, "div(sub(rank(%s), rank(%s)), %s)" % (REV, s, TRAD))
    push("B_MUL_" + sn, "div(mul(rank(%s), rank(%s)), %s)" % (REV, s, TRAD))
    push("B_W075_" + sn, "div(add(rank(%s), mul(0.75, rank(%s))), %s)" % (REV, s, TRAD))
    push("B_W050_" + sn, "div(add(rank(%s), mul(0.5, rank(%s))), %s)" % (REV, s, TRAD))

# ---- C：两个非反转信号 ----
pairs = [('corr_cv20', 'vola20'), ('corr_lv20', 'vola20'), ('autocorr20', 'range20'),
         ('distmax20', 'distmin20'), ('volratio', 'corr_cv20'), ('range20', 'vola20')]
for a, b in pairs:
    push("C_ADD", "div(add(rank(%s), rank(%s)), %s)" % (SIG[a], SIG[b], TRAD))
    push("C_SUB", "div(sub(rank(%s), rank(%s)), %s)" % (SIG[a], SIG[b], TRAD))
    push("C_MUL", "div(mul(rank(%s), rank(%s)), %s)" % (SIG[a], SIG[b], TRAD))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("y%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("y%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
