# -*- coding: utf-8 -*-
"""第 7 轮：检验「低价格效应是不是报价粘滞造成的」。

第 6 轮发现：价格最低 10% 的股票，1 分钟反转 IC 两段样本都在 0.06~0.09。
红旗：再叠加「高成交量」不但没提升，反而略降。

两种解释：
  H1 真实过度反应：低价股散户多、反应过度，反转可交易。
  H2 报价粘滞：低价股报价更新慢，中间价粘住再跳，IC 是假象、不可交易。

判别实验（都是**在低价股内部**做二次分层）：
  V_*  按成交量分四档：若 H2，最活跃档的 IC 应显著下降
  D_*  按成交额（金额）分四档：同上，但金额更贴近可交易性
  G_*  按分钟内振幅分档：H1 预测「振幅越大反转越强」，H2 预测相反或无差别
  S_*  按价格波动分档
  H_*  低价股内不同反转窗口的衰减

用法：python3 gen_search7.py [out.txt]
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r7.txt'

R1 = "sub(div(close,delay(close,1)),1)"
LOWPX = "lt(rank(close),0.20)"


def dev(w):
    return "neg(sub(div(close,ts_mean(close,%d)),1))" % w


def lowd(w):
    return "neg(sub(div(low,ts_mean(low,%d)),1))" % w


def dec(w):
    return "neg(decay(%s,%d))" % (R1, w)


BASE = dev(8)

# (低分位, 高分位, 变量)
BUCKETS = [
    ('vol', "rank(ts_mean(volume,20))", [(0.00, 0.25), (0.25, 0.50), (0.50, 0.75), (0.75, 1.00)]),
    ('amt', "rank(ts_mean(amount,20))", [(0.00, 0.25), (0.25, 0.50), (0.50, 0.75), (0.75, 1.00)]),
    ('rng', "rank(div(sub(high,low),close))", [(0.00, 0.25), (0.25, 0.50), (0.50, 0.75), (0.75, 1.00)]),
    ('vola', "rank(ts_std(%s,20))" % R1, [(0.00, 0.25), (0.25, 0.50), (0.50, 0.75), (0.75, 1.00)]),
]

out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


def mask(lo, hi, var):
    if lo <= 0.0:
        return "lt(%s,%.2f)" % (var, hi)
    if hi >= 1.0:
        return "gt(%s,%.2f)" % (var, lo)
    return "mul(gt(%s,%.2f), lt(%s,%.2f))" % (var, lo, var, hi)


for name, var, buckets in BUCKETS:
    for i, (lo, hi) in enumerate(buckets):
        cond = "mul(%s, %s)" % (LOWPX, mask(lo, hi, var))
        push("%s_Q%d" % (name.upper(), i + 1), "div(%s, %s)" % (BASE, cond))

# 低价股内的信号定义（窗口衰减）
for w in [1, 2, 3, 5, 8, 10, 15, 20, 30]:
    push("H_DEV", "div(%s, %s)" % (dev(w), LOWPX))
for w in [2, 3, 5, 8]:
    push("H_DEC", "div(%s, %s)" % (dec(w), LOWPX))

# 低价股内，成交量最高档 + 不同窗口（H2 的关键判别）
HI = "mul(%s, gt(rank(ts_mean(volume,20)),0.75))" % LOWPX
for w in [2, 3, 5, 8, 10, 20]:
    push("V_HI_DEV", "div(%s, %s)" % (dev(w), HI))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("x%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("x%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
