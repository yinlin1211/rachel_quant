# -*- coding: utf-8 -*-
"""第 5 轮：在 top-5% 成交量子集内，换基础信号与第二维度。

已知：
  - 硬阈值 0.95 在样本外 IC=0.0537；软权重（连续加权）只有 0.028，说明
    增益来自「硬选择顶部尾部」，不是「按流动性等比例放大」。
  - 相对量能（volume/均量）失效 → 起作用的是**绝对流动性水平**，不是放量。

本轮问：
  A_*  顶部尾部里，哪种反转定义最强（参照价 / 窗口）？
  C_*  顶部尾部里，第二个维度（涨跌方向、日内位置、量价相关）能否叠加？
  E_*  双条件：top-5% 量 × 某种状态，是否比单条件强？
  F_*  日频相对量（ts_rank(volume,240)）与横截面量排名的组合。

用法：python3 gen_search5.py [out.txt]
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r5.txt'

R1 = "sub(div(close,delay(close,1)),1)"
TOP = "gt(rank(ts_mean(volume,20)),0.95)"


def lowd(w):
    return "neg(sub(div(low,ts_mean(low,%d)),1))" % w


def dev(w):
    return "neg(sub(div(close,ts_mean(close,%d)),1))" % w


def vwd(w):
    return "neg(sub(div(close,ts_mean(vwap,%d)),1))" % w


def dec(w):
    return "neg(decay(%s,%d))" % (R1, w)


out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


# ---- A：基础信号定义扫描（top-5%）----
BASES = {
    'low2': lowd(2), 'low10': lowd(10), 'low15': lowd(15), 'low20': lowd(20),
    'dev8': dev(8), 'dev10': dev(10), 'dev15': dev(15),
    'dec2': dec(2), 'dec10': dec(10), 'dec15': dec(15),
    'vwd5': vwd(5), 'vwd10': vwd(10), 'vwd20': vwd(20),
    'ret1': "neg(%s)" % R1,
}
for bn, b in BASES.items():
    push("A_" + bn, "div(%s, %s)" % (b, TOP))

# ---- C：第二个维度（同一子集内）----
SIG2 = {
    'dir': "sign(%s)" % R1,
    'daypos': "sub(div(close,ts_max(high,240)),1)",
    'daylow': "sub(div(low,ts_min(low,240)),1)",
    'corr_cv': "ts_corr(close,volume,20)",
    'corr_lv': "ts_corr(low,volume,20)",
    'argmax': "ts_argmax(close,20)",
    'argmin': "ts_argmin(close,20)",
    'vola': "ts_std(%s,20)" % R1,
}
for sn, s in SIG2.items():
    for bn in ['low5', 'dec3', 'dev5']:
        base = {'low5': lowd(5), 'dec3': dec(3), 'dev5': dev(5)}[bn]
        push("C_ADD_" + sn, "div(add(rank(%s), rank(%s)), %s)" % (base, s, TOP))
        push("C_SUB_" + sn, "div(sub(rank(%s), rank(%s)), %s)" % (base, s, TOP))
        push("C_MUL_" + sn, "div(mul(rank(%s), rank(%s)), %s)" % (base, s, TOP))

# ---- E：双条件 ----
COND2 = {
    'bigmove': "gt(rank(abs(%s)),0.5)" % R1,
    'bigmove8': "gt(rank(abs(%s)),0.8)" % R1,
    'abovevwap': "gt(close,vwap)",
    'belowvwap': "lt(close,vwap)",
    'hi_range': "gt(rank(div(sub(high,low),close)),0.5)",
}
for cn, c in COND2.items():
    cond = "mul(%s, %s)" % (TOP, c)
    for bn in ['low5', 'dec3']:
        base = {'low5': lowd(5), 'dec3': dec(3)}[bn]
        push("E_" + cn, "div(%s, %s)" % (base, cond))

# ---- F：日频相对量 ----
for t in [0.7, 0.8, 0.9]:
    cond = "gt(ts_rank(volume,240),%.1f)" % t
    for bn in ['low5', 'dec3']:
        base = {'low5': lowd(5), 'dec3': dec(3)}[bn]
        push("F_tsrank%d" % int(t * 10), "div(%s, %s)" % (base, cond))
    # 横截面量排名 × 日频相对量
    cond2 = "mul(%s, %s)" % (TOP, cond)
    push("F_combo%d" % int(t * 10), "div(%s, %s)" % (lowd(5), cond2))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("v%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("v%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
