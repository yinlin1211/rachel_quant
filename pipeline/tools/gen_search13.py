# -*- coding: utf-8 -*-
"""第 13 轮：暴跌反弹事件的细化。

第 12 轮：z < −2 的暴跌股反弹，可交易池内 OOS IC1=0.118，但覆盖率 1%、IR≈1.0。
本轮问：
  T_*  阈值放在哪里最划算（z < −1.5 / −2 / −2.5 / −3）？
  M_*  多分钟累计跌幅（3/5 分钟）是否比单分钟更可靠？
  C_*  叠加条件（放量 / 大振幅 / 之前已在下跌趋势中）能否提高 IR？
  U_*  暴涨（z > 2）之后是延续还是不反转？用方向因子验证。

全部在成交额前 50% 的池内统计。
"""
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'search_r13.txt'

R1 = "sub(div(close,delay(close,1)),1)"
R3 = "sub(div(close,delay(close,3)),1)"
R5 = "sub(div(close,delay(close,5)),1)"
TRAD = "gt(rank(ts_mean(amount,20)),0.5)"
BASE = "neg(sub(div(low,ts_mean(low,5)),1))"

Z1 = "div(%s, add(ts_std(%s,20),0.00001))" % (R1, R1)
Z3 = "div(%s, add(ts_std(%s,20),0.00001))" % (R3, R3)
Z5 = "div(%s, add(ts_std(%s,20),0.00001))" % (R5, R5)

out, seen = [], set()


def push(tag, e):
    if e in seen:
        return
    seen.add(e)
    out.append((tag, e))


def m(*parts):
    r = parts[0]
    for p in parts[1:]:
        r = "mul(%s, %s)" % (r, p)
    return r


# ---- T：阈值 ----
for t in [-1.5, -2.0, -2.5, -3.0]:
    cond = m(TRAD, "lt(%s,%.1f)" % (Z1, t))
    push("T_%s" % str(abs(t)).replace('.', ''), "div(%s, %s)" % (BASE, cond))

# ---- M：多分钟跌幅 ----
for name, z in [('z3', Z3), ('z5', Z5)]:
    for t in [-2.0, -2.5]:
        cond = m(TRAD, "lt(%s,%.1f)" % (z, t))
        push("M_%s_%s" % (name, str(abs(t)).replace('.', '')),
             "div(%s, %s)" % (BASE, cond))

# ---- C：暴跌 + 叠加条件 ----
CRASH = m(TRAD, "lt(%s,-2.0)" % Z1)
CONDS = {
    'hivol': "gt(div(volume, add(ts_mean(volume,20),1)),1.2)",
    'hirange': "gt(div(sub(high,low),close),0.006)",
    'downtrend': "lt(%s,0)" % ("sub(div(close,ts_mean(close,20)),1)"),
    'uptrend': "gt(%s,0)" % ("sub(div(close,ts_mean(close,20)),1)"),
    'lowvol': "lt(rank(ts_std(%s,20)),0.5)" % R1,
}
for cn, c in CONDS.items():
    push("C_" + cn, "div(%s, %s)" % (BASE, m(CRASH, c)))

# ---- U：暴涨之后 ----
SPIKE = m(TRAD, "gt(%s,2.0)" % Z1)
push("U_up_mom", "div(%s, %s)" % (R1, SPIKE))       # 正 = 延续
push("U_up_rev", "div(%s, %s)" % ("neg(%s)" % R1, SPIKE))
push("U_up_base", "div(%s, %s)" % (BASE, SPIKE))    # 用反转因子测

# 暴跌用「当分钟收益」直接做（对照 BASE）
push("U_down_raw", "div(neg(%s), %s)" % (R1, CRASH))

with open(OUT, 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("n%05d|%s\n" % (k + 1, e))
with open(OUT + ".map", 'w') as f:
    for k, (tag, e) in enumerate(out):
        f.write("n%05d,%s\n" % (k + 1, tag))
print("generated %d -> %s" % (len(out), OUT))
