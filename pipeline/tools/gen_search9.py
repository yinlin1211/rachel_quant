# -*- coding: utf-8 -*-
"""第 9 轮：两条线。

线 1（IR）：第 8 轮发现「反转 × 量比」把 IR 从 2.34 拉到 5.59，但 IC 从 0.039 掉到 0.027。
  要确认这是稳定的权衡还是偶然，做法是扫窗口和权重，看 IC/IR 曲面的形状。

线 2（基准）：第 8 轮发现用 low 比用 close 高 60%。
  本轮把「low」拆开看：low / 滚动最低 / close / 两者混合，确认 0.039 是不是上限。

全部在成交额前 50% 的股票池内统计。
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r9.txt'

R1 = "sub(div(close,delay(close,1)),1)"
TRAD = "gt(rank(ts_mean(amount,20)),0.5)"

out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


def rev_low(w):
    return "neg(sub(div(low,ts_mean(low,%d)),1))" % w


def rev_min(w):
    return "neg(sub(div(low,ts_min(low,%d)),1))" % w


def rev_close(w):
    return "neg(sub(div(close,ts_mean(close,%d)),1))" % w


def volratio(f, s):
    return "div(ts_mean(volume,%d),ts_mean(volume,%d))" % (f, s)


# ---- 线 1：反转 × 量比 的窗口/权重扫描 ----
for w in [3, 5, 8, 10]:
    for f, s in [(5, 60), (5, 120), (5, 240), (10, 60), (10, 240), (20, 60)]:
        vr = volratio(f, s)
        base = rev_low(w)
        push("L1_MUL_%d_%d_%d" % (w, f, s),
             "div(mul(rank(%s), rank(%s)), %s)" % (base, vr, TRAD))
        push("L1_ADD_%d_%d_%d" % (w, f, s),
             "div(add(rank(%s), rank(%s)), %s)" % (base, vr, TRAD))
        push("L1_W05_%d_%d_%d" % (w, f, s),
             "div(add(rank(%s), mul(0.5, rank(%s))), %s)" % (base, vr, TRAD))

# ---- 线 2：基础信号的定义 ----
for w in [3, 5, 8, 10, 20]:
    push("L2_LOW", "div(%s, %s)" % (rev_low(w), TRAD))
    push("L2_MIN", "div(%s, %s)" % (rev_min(w), TRAD))
    push("L2_CLOSE", "div(%s, %s)" % (rev_close(w), TRAD))
    # 混合：low 与 close 的均值
    mix = "neg(sub(div(mul(0.5, add(low,close)),ts_mean(close,%d)),1))" % w
    push("L2_MIX", "div(%s, %s)" % (mix, TRAD))
    # close 相对滚动最低（不是自己的均值）
    push("L2_CLOSEMIN", "div(neg(sub(div(close,ts_min(low,%d)),1)), %s)" % (w, TRAD))

# ---- 线 2 补充：low 反转 + 量比的最终候选（成交额前 50%）----
for w in [3, 5, 8]:
    base = rev_low(w)
    push("L2_LOWVR", "div(mul(rank(%s), rank(%s)), %s)" % (base, volratio(5, 60), TRAD))
    push("L2_LOWVR2", "div(add(rank(%s), mul(0.5,rank(%s))), %s)" % (base, volratio(5, 60), TRAD))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("a%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("a%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
