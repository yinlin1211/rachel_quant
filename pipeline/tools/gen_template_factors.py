# -*- coding: utf-8 -*-
"""生成 1000 个有明确金融含义的因子（模板实例化，不做两两拼凑）。

每个模板对应一个经济学假设（反转、动量、放量、量价背离、波动、位置……），
窗口取 3–120 分钟，每个模板轮流取，保证覆盖面。

输出：
  <out.txt>        id|expr
  <out>.catalog    family,meaning,count

用法：python3 gen_template_factors.py [out.txt] [N=1000]
"""
import sys
import gen_factors as g

OUT = sys.argv[1] if len(sys.argv) > 1 else 'template_factors.txt'
N = int(sys.argv[2]) if len(sys.argv) > 2 else 1000

# 与 TEMPLATES 一一对应
MEANINGS = [
    "分钟K线实体占振幅：单边推进 vs 多空拉锯",
    "收盘价相对成交均价的偏离：买盘强弱",
    "分钟涨跌幅：短期动量/反转",
    "上影线占比：上方抛压",
    "下影线占比：下方承接",
    "振幅占价格：日内波动强度",
    "滞后 w 分钟收益：动量/反转",
    "价格偏离 w 分钟均线：均值回复",
    "价格在 w 分钟区间中的位置：突破/超买超卖",
    "收盘价在 w 分钟内的时序分位：趋势强度",
    "w 分钟内最高点距今多久：新高动能",
    "w 分钟内最低点距今多久：新低动能",
    "开盘价相对 w 分钟前收盘的跳空幅度",
    "均价相对收盘的偏离（反向）",
    "收盘相对均价的偏离（正向）",
    "最高价偏离其 w 分钟均线：上沿突破",
    "最低价偏离其 w 分钟均线：下沿破位",
    "成交量相对 w 分钟均量的倍数：放量/缩量",
    "成交量波动率：量能稳定性",
    "成交量在 w 分钟内的分位：相对活跃度",
    "成交量相对 w 分钟前的变化率",
    "成交额相对 w 分钟均额的倍数",
    "成交额相对 w 分钟前的变化率",
    "w 分钟内收益率的波动：已实现波动率",
    "价格变异系数：波动/价位",
    "振幅的波动：波动率的波动",
    "量价相关性（分位版）：量能与价格同步性",
    "量与原收益的相关：放量上涨/下跌",
    "高价与成交量的相关：追高资金",
    "均价与成交量的相关：均价支撑",
    "量分位与均价分位的相关",
    "振幅与成交量的相关：波动放量",
    "收盘与成交量的相关",
    "开盘与成交量的相关",
    "收益率的线性衰减加权：近期动量",
    "收益率的 w 分钟均值：平滑动量",
    "短均线 / 长均线 - 1：均线交叉",
    "短均量 / 长均量 - 1：量能趋势",
    "短周期量 / 长周期量：量能比",
]


def main():
    groups = []
    for ti, tpl in enumerate(g.TEMPLATES):
        arr = []
        if '{w2}' in tpl:
            for i, w in enumerate(g.WINDOWS):
                for w2 in g.WINDOWS[i + 1:]:
                    arr.append(g.inst(tpl, w, w2))
        else:
            for w in g.WINDOWS:
                arr.append(g.inst(tpl, w))
        groups.append(arr)

    def variants(e):
        return [e, "rank(%s)" % e, "neg(%s)" % e, "rank(neg(%s))" % e]

    out, seen = [], set()

    # 轮转：先每个族出基础式，再依次上 rank/neg 包装，保证族间均匀
    level = 0
    while len(out) < N:
        added = False
        for ti, arr in enumerate(groups):
            for e in arr:
                v = variants(e)
                if level >= len(v):
                    break
                x = v[level]
                c = g.canon(x)
                if c in seen:
                    continue
                seen.add(c)
                out.append((ti, e, x))
                added = True
                if len(out) >= N:
                    break
            if len(out) >= N:
                break
        if not added:
            break
        level += 1

    with open(OUT, 'w') as f:
        for k, (ti, base, x) in enumerate(out):
            f.write("t%05d|%s\n" % (k + 1, x))

    with open(OUT + '.map', 'w') as f:
        f.write("id,family\n")
        for k, (ti, base, x) in enumerate(out):
            f.write("t%05d,T%02d\n" % (k + 1, ti + 1))

    cat = OUT + '.catalog'
    cnt = {}
    for ti, _, _ in out:
        cnt[ti] = cnt.get(ti, 0) + 1
    with open(cat, 'w') as f:
        f.write("family,meaning,count,example\n")
        for ti, m in enumerate(MEANINGS):
            if ti < len(g.TEMPLATES):
                f.write('T%02d,"%s",%d,"%s"\n' % (ti + 1, m, cnt.get(ti, 0), groups[ti][0]))

    print('generated %d factors -> %s' % (len(out), OUT))
    print('families=%d  catalog=%s' % (len(g.TEMPLATES), cat))


if __name__ == '__main__':
    main()
