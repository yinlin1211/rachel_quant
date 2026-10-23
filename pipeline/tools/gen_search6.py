# -*- coding: utf-8 -*-
"""第 6 轮：把「价格水平」当成条件变量（第 5 轮控制实验发现的）。

控制实验结果（2026-01 / 2026-07）：
  成交量前 5%  : 0.0621 / 0.0609
  价格最低 5%  : 0.0770 / **0.1038**   ← 比成交量更强
  价格最高 5%  : 0.0157 / 0.0294
  成交量中间 10%: 0.0174 / —
→ 真正的条件变量可能不是「流动性」，而是**价格水平 / 最小变动价位**。

本轮问：
  P_*  价格分位曲线：底部 1% / 2% / 5% / 10% / 20% / 30%，IC 在哪一档见顶？
  Q_*  低价格 × 高成交量 的组合阈值
  R_*  低价格子集里，哪种反转定义最强
  S_*  剔除极端振幅（可能不可交易）后还剩多少

用法：python3 gen_search6.py [out.txt]
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r6.txt'

R1 = "sub(div(close,delay(close,1)),1)"


def lowd(w):
    return "neg(sub(div(low,ts_mean(low,%d)),1))" % w


def dev(w):
    return "neg(sub(div(close,ts_mean(close,%d)),1))" % w


def dec(w):
    return "neg(decay(%s,%d))" % (R1, w)


out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


BASE = dev(8)
PRICE = "rank(close)"
VOLU = "rank(ts_mean(volume,20))"
VOLA = "rank(ts_std(%s,20))" % R1

# ---- P：价格分位曲线 ----
for t in [0.01, 0.02, 0.05, 0.10, 0.20, 0.30, 0.50]:
    push("P_price%d" % int(t * 100),
         "div(%s, lt(%s,%.2f))" % (BASE, PRICE, t))

# ---- Q：价格 × 成交量 ----
for pt in [0.05, 0.10, 0.20, 0.30, 0.50]:
    for vt in [0.5, 0.8, 0.9]:
        push("Q_p%dv%d" % (int(pt * 100), int(vt * 10)),
             "div(%s, mul(lt(%s,%.2f), gt(%s,%.1f)))" % (BASE, PRICE, pt, VOLU, vt))

# ---- R：低价格子集内的信号定义 ----
LOWPX = "lt(%s,0.20)" % PRICE
RB = {
    'low3': lowd(3), 'low5': lowd(5), 'low8': lowd(8), 'low15': lowd(15),
    'dev3': dev(3), 'dev5': dev(5), 'dev8': dev(8), 'dev10': dev(10),
    'dev15': dev(15), 'dev20': dev(20),
    'dec2': dec(2), 'dec3': dec(3), 'dec5': dec(5),
    'ret1': "neg(%s)" % R1,
}
for bn, b in RB.items():
    push("R_" + bn, "div(%s, %s)" % (b, LOWPX))

# ---- S：剔除极端振幅 / 极端波动（可交易性过滤） ----
for sig, name in [(VOLA, 'vola'), ("rank(div(sub(high,low),close))", 'range')]:
    for t in [0.5, 0.8]:
        cond = "mul(%s, lt(%s,%.1f))" % (LOWPX, sig, t)
        push("S_%s%d" % (name, int(t * 10)), "div(%s, %s)" % (BASE, cond))

# ---- 低价格 × 高成交量的信号定义扫描 ----
LPV = "mul(lt(%s,0.20), gt(%s,0.8))" % (PRICE, VOLU)
for bn in ['low5', 'dev5', 'dec3', 'ret1']:
    push("QS_" + bn, "div(%s, %s)" % (RB[bn], LPV))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("w%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("w%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
