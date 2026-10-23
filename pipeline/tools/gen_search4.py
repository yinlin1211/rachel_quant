# -*- coding: utf-8 -*-
"""第 4 轮：条件变量本身的选择 + 子集内独立信号。

上一轮：阈值越严 IC 越高（0.5→0.9 单调，两个时段都成立）。
本轮问：
  U_*  条件用哪个窗口/变量最稳？绝对均量（5/10/20/60）vs 相对量能（volume/均量）
       阈值 0.85 / 0.95 —— 曲线顶端是继续上升还是转平/反转？
  V_*  换反转的参照价（low / vwap / close）在极端子集内谁更强？
  W_*  在子集内叠加一个与反转无关的信号（量价相关、振幅、波动、bar 位置），
       能否在同样覆盖率下把 IC 推过 0.05？

用法：python3 gen_search4.py [out.txt]
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r4.txt'

R1 = "sub(div(close,delay(close,1)),1)"


def lowd(w):
    return "neg(sub(div(low,ts_mean(low,%d)),1))" % w


def dev(w):
    return "neg(sub(div(close,ts_mean(close,%d)),1))" % w


def vwd(w):
    return "neg(sub(div(close,ts_mean(vwap,%d)),1))" % w


def dec(w):
    return "neg(decay(%s,%d))" % (R1, w)


BASES = {
    'low3': lowd(3), 'low5': lowd(5), 'low8': lowd(8),
    'dev5': dev(5), 'dec3': dec(3), 'vwd5': vwd(5),
}

out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


# ---- U：条件变量的窗口扫描 ----
CONDS = {}
for w in [5, 10, 20, 60]:
    CONDS['volu%d' % w] = "rank(ts_mean(volume,%d))" % w
    CONDS['amt%d' % w] = "rank(ts_mean(amount,%d))" % w
CONDS['relvol60'] = "rank(div(volume, add(ts_mean(volume,60),1)))"
CONDS['relamt60'] = "rank(div(amount, add(ts_mean(amount,60),1)))"
CONDS['relvol20'] = "rank(div(volume, add(ts_mean(volume,20),1)))"

for cn, cond in CONDS.items():
    for t in [0.85, 0.9, 0.95]:
        for bn in ['low5', 'dev5', 'dec3']:
            push("U_%s_%d" % (cn, int(t * 100)),
                 "div(%s, gt(%s,%.2f))" % (BASES[bn], cond, t))

# ---- V：极端子集内的参照价 ----
TOP = "gt(rank(ts_mean(volume,20)),0.9)"
for bn in ['low3', 'low5', 'low8', 'dev5', 'vwd5', 'dec3']:
    push("V_REF", "div(%s, %s)" % (BASES[bn], TOP))

# ---- W：子集内叠加独立信号 ----
SIGS = {
    'corr_cv': "rank(ts_corr(close,volume,20))",
    'corr_lv': "rank(ts_corr(low,volume,20))",
    'range': "rank(div(sub(high,low),close))",
    'barpos': "rank(div(sub(close,low),add(sub(high,low),0.001)))",
    'vola': "rank(ts_std(%s,20))" % R1,
    'intra': "rank(sub(div(close,vwap),1))",
}
for sn, sig in SIGS.items():
    for bn in ['low5', 'dev5', 'dec3']:
        push("W_ADD_%s" % sn,
             "div(add(rank(%s), %s), %s)" % (BASES[bn], sig, TOP))
        push("W_SUB_%s" % sn,
             "div(sub(rank(%s), %s), %s)" % (BASES[bn], sig, TOP))
        push("W_MUL_%s" % sn,
             "div(mul(rank(%s), %s), %s)" % (BASES[bn], sig, TOP))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("u%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("u%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
