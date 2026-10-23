# -*- coding: utf-8 -*-
"""生成因子表达式（DSL）。

原则：
1. 所有表达式来自有经济含义的模板，不做随机符号拼接；
2. 量纲合法：只用无量纲比值、同量纲相减、标量缩放；
3. 时序算子只向过去看，不存在未来函数；
4. 规范化去重。

输出：每行 "id|expr"，供 bin/alpha_expr 求值。

用法：python3 gen_factors.py <out.txt> [目标数量=10000]
"""
import sys
import re

OUT = sys.argv[1] if len(sys.argv) > 1 else 'factors.txt'
TARGET = int(sys.argv[2]) if len(sys.argv) > 2 else 10000

WINDOWS = [3, 5, 8, 10, 15, 20, 30, 45, 60, 90, 120]

# 经济模板：全部无量纲或有明确经济含义
TEMPLATES = [
    # 价格结构
    "div(sub(close,open), add(sub(high,low),0.001))",              # 实体占振幅
    "div(sub(close,vwap), vwap)",                                  # 收盘偏离均价
    "div(sub(close,open), open)",                                  # 分钟涨跌
    "div(sub(high,close), add(sub(high,low),0.001))",             # 上影线占比
    "div(sub(close,low), add(sub(high,low),0.001))",              # 下影线占比
    "div(sub(high,low), close)",                                   # 振幅
    "sub(div(close, delay(close,{w})),1)",                         # 滞后收益
    "sub(div(close, ts_mean(close,{w})),1)",                       # 偏离均线
    "div(sub(close, ts_min(close,{w})), add(sub(ts_max(close,{w}), ts_min(close,{w})),0.001))",  # 区间位置
    "ts_rank(close,{w})",                                          # 价格分位
    "ts_argmax(close,{w})",                                        # 高点距今
    "ts_argmin(close,{w})",                                        # 低点距今
    "sub(div(open, delay(close,{w})),1)",                          # 跳空
    "sub(div(vwap, close),1)",
    "sub(div(close, vwap),1)",
    "sub(div(high, ts_mean(high,{w})),1)",
    "sub(div(low, ts_mean(low,{w})),1)",
    # 量能
    "div(volume, ts_mean(volume,{w}))",                            # 放量倍数
    "div(ts_std(volume,{w}), add(ts_mean(volume,{w}),1))",        # 量的波动
    "ts_rank(volume,{w})",                                         # 量的分位
    "sub(div(volume, delay(volume,{w})),1)",                       # 量的变化
    "div(amount, ts_mean(amount,{w}))",
    "sub(div(amount, delay(amount,{w})),1)",
    # 波动
    "ts_std(sub(div(close,delay(close,1)),1),{w})",                # 收益波动
    "div(ts_std(close,{w}), ts_mean(close,{w}))",                  # 价格变异系数
    "ts_std(sub(div(high,low),close),{w})",                        # 振幅波动
    # 量价相关
    "ts_corr(rank(volume), rank(close), {w})",
    "ts_corr(volume, sub(div(close,delay(close,1)),1), {w})",
    "ts_corr(high, volume, {w})",
    "ts_corr(vwap, volume, {w})",
    "ts_corr(rank(volume), rank(vwap), {w})",
    "ts_corr(sub(high,low), volume, {w})",
    "ts_corr(close, volume, {w})",
    "ts_corr(open, volume, {w})",
    # 时序加权
    "decay(sub(div(close,delay(close,1)),1), {w})",
    "ts_mean(sub(div(close,delay(close,1)),1), {w})",
    # 双窗口
    "sub(div(ts_mean(close,{w}), ts_mean(close,{w2})),1)",
    "sub(div(ts_mean(volume,{w}), ts_mean(volume,{w2})),1)",
    "div(ts_sum(volume,{w}), ts_sum(volume,{w2}))",
]

WRAPPERS = [("", ""), ("rank", "rank"), ("neg", "neg"), ("rank(neg", "rank(neg")]


def inst(tpl, w, w2=None):
    if '{w2}' in tpl and w2 is None:
        return None
    return tpl.format(w=w, w2=w2 if w2 is not None else w)


def canon(e):
    """粗糙规范化，仅用于去掉完全相同的字符串"""
    return re.sub(r'\s+', '', e)


def main():
    bases = []
    for tpl in TEMPLATES:
        if '{w2}' in tpl:
            for i, w in enumerate(WINDOWS):
                for w2 in WINDOWS[i + 1:]:
                    e = inst(tpl, w, w2)
                    if e:
                        bases.append(e)
        else:
            for w in WINDOWS:
                bases.append(inst(tpl, w))

    out = []
    seen = set()

    def push(e):
        c = canon(e)
        if c in seen:
            return False
        seen.add(c)
        out.append(e)
        return True

    # 第一层：模板 × 包装
    for b in bases:
        push(b)
        push("rank(%s)" % b)
        push("neg(%s)" % b)
        push("rank(neg(%s))" % b)
        push("decay(%s, 5)" % b)
        push("ts_mean(%s, 3)" % b)

    # 第二层：两两组合（无量纲之间，量纲合法）
    n = len(bases)
    step = max(1, n * n // max(1, (TARGET - len(out)) * 2))
    for i in range(n):
        if len(out) >= TARGET:
            break
        for j in range(i + 1, n):
            if len(out) >= TARGET:
                break
            if (i * 7919 + j * 104729) % 65 != 0:   # 确定性抽样，避免全量爆炸
                continue
            a, b = bases[i], bases[j]
            push("mul(%s, %s)" % (a, b))
            push("sub(rank(%s), rank(%s))" % (a, b))
            push("div(add(%s,0.001), add(abs(%s),0.001))" % (a, b))

    with open(OUT, 'w') as f:
        for k, e in enumerate(out[:TARGET]):
            f.write("g%05d|%s\n" % (k + 1, e))
    print('generated %d expressions -> %s' % (min(len(out), TARGET), OUT))
    print('templates=%d  windows=%d  base_instances=%d' % (len(TEMPLATES), len(WINDOWS), len(bases)))


if __name__ == '__main__':
    main()
