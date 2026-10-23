# -*- coding: utf-8 -*-
"""定向搜索：围绕「分钟级均值回复」做风险归一化与条件化，目标是 IC ≥ 0.05。

每一族对应一个可证伪的假设：
  Z*  : 反转信号按个股已实现波动率归一化后，IC 应上升（风险调整）
  X*  : 反转只在低量/高量条件下成立（交互项）
  M*  : 多尺度反转之差能分离「短期超跌」与「中期趋势」
  B*  : 收盘价在当根 K 线中的位置（bar position）是更纯的微观结构反转

用法：python3 gen_search.py [out.txt]
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r1.txt'

W = [2, 3, 5, 8, 10, 15, 20, 30, 60]
L = [5, 10, 20, 60]

R1 = "sub(div(close,delay(close,1)),1)"
VOL = "add(ts_std(%s,{L}),0.0001)" % R1


def dev(w):
    return "sub(div(close,ts_mean(close,%d)),1)" % w


def lowdev(w):
    return "sub(div(low,ts_mean(low,%d)),1)" % w


def vwapdev(w):
    return "sub(div(close,ts_mean(vwap,%d)),1)" % w


def ret(w):
    return "sub(div(close,delay(close,%d)),1)" % w


out = []
seen = set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


# F1 价格偏离均线，按已实现波动率归一化（风险调整反转）
for w in W:
    for l in L:
        d = dev(w)
        push("Z_DEV", "neg(div(%s, %s))" % (d, VOL.format(L=l)))
        push("Z_DEV_RANK", "rank(neg(div(%s, %s)))" % (d, VOL.format(L=l)))

# F2 直接收益反转，按波动率归一化
for w in [1, 2, 3, 5, 8, 10, 20, 30]:
    for l in L:
        push("Z_RET", "neg(div(%s, %s))" % (ret(w), VOL.format(L=l)))

# F3 最低价破位，按波动率归一化（当前最强族的风险调整版）
for w in W:
    for l in L:
        push("Z_LOW", "neg(div(%s, %s))" % (lowdev(w), VOL.format(L=l)))

# F4 vwap 偏离，按波动率归一化
for w in W:
    for l in L:
        push("Z_VWAP", "neg(div(%s, %s))" % (vwapdev(w), VOL.format(L=l)))

# F5/F6 反转 × 量能条件（放量 / 缩量）
for w in W:
    d = dev(w)
    for W2 in [10, 20, 60]:
        vr = "div(ts_mean(volume,%d),ts_mean(volume,%d))" % (w, W2)
        push("X_HIVOL", "neg(mul(%s, rank(%s)))" % (d, vr))
        push("X_LOVOL", "neg(mul(%s, sub(2,rank(%s))))" % (d, vr))

# F7 收盘在当根 K 线中的位置
pos = "div(sub(close,low),add(sub(high,low),0.001))"
push("B_POS", "neg(%s)" % pos)
push("B_POS_RANK", "rank(neg(%s))" % pos)
for w in [5, 10, 20]:
    vr = "div(ts_mean(volume,%d),ts_mean(volume,20))" % w
    push("B_POS_VOL", "neg(mul(%s, rank(%s)))" % (pos, vr))
    push("B_POS_VOL2", "neg(mul(rank(%s), rank(%s)))" % (pos, vr))

# F8 多尺度反转之差
for i, w1 in enumerate(W):
    for w2 in W[i + 1:]:
        if w2 > w1 * 4:
            continue
        for l in [10, 20]:
            push("M_MULTI", "neg(div(sub(%s,%s), %s))" % (dev(w1), dev(w2), VOL.format(L=l)))

# F9 衰减加权反转，按波动率归一化
for w in [3, 5, 8, 10, 20]:
    for l in [10, 20]:
        push("D_DECAY", "neg(div(decay(%s,%d), %s))" % (R1, w, VOL.format(L=l)))

# F10 bar 位置 × 多尺度
for w in [3, 5, 10, 20]:
    push("B_MULTI", "neg(div(sub(%s,%s), %s))" % (pos, "ts_mean(%s,%d)" % (pos, w), VOL.format(L=20)))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("s%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("s%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
