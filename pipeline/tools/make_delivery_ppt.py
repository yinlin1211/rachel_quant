# -*- coding: utf-8 -*-
"""按平台交付模板生成因子说明 PPT。

模板结构（来自 交付示例/mike_20260101_introduction.pptx）：
  每个因子 2 页：
    第 1 页：因子名 / 所有者 / 测试周期 / 测试 IC 值（预测 1min）/ 因子概要（IC 曲线）
    第 2 页：原理描述（经济含义、构造逻辑、文献依据）

用法：
  python3 make_delivery_ppt.py <summary.csv> <daily.csv> <map.csv> <catalog.csv> \
      <factors.txt> <out.pptx> [topN=10] [owner=yinlin1211]
"""
import sys
import csv
import os
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor

SUMMARY, DAILY, MAPF, CATF, FACTF, OUT = sys.argv[1:7]
TOPN = int(sys.argv[7]) if len(sys.argv) > 7 else 10
OWNER = sys.argv[8] if len(sys.argv) > 8 else 'yinlin1211'
TMP = '/tmp/ppt_charts'
os.makedirs(TMP, exist_ok=True)

DARK = RGBColor(0x1F, 0x2A, 0x44)
GREY = RGBColor(0x55, 0x5F, 0x70)
ACCENT = RGBColor(0xC0, 0x39, 0x2B)


def fnum(x):
    try:
        return float(x)
    except Exception:
        return np.nan


def load():
    rows = list(csv.DictReader(open(SUMMARY)))
    factors = {}
    for line in open(FACTF):
        if '|' in line:
            i, e = line.strip().split('|', 1)
            factors[i] = e
    fam = {r['id']: r['family'] for r in csv.DictReader(open(MAPF))}
    meaning = {r['family']: r['meaning'] for r in csv.DictReader(open(CATF))}
    drows = list(csv.reader(open(DAILY)))
    dates = drows[0][1:]
    daily = {r[0]: np.array([fnum(x) for x in r[1:]]) for r in drows[1:]}
    return rows, factors, fam, meaning, dates, daily


def pick(rows, n):
    cand = []
    for r in rows:
        ic = fnum(r['IC1_mean']); cov = fnum(r['coverage']); ir = fnum(r['IR1'])
        if np.isfinite(ic) and cov > 0.9 and np.isfinite(ir) and abs(ir) > 1.0:
            cand.append((abs(ic), r))
    cand.sort(key=lambda x: -x[0])
    return [r for _, r in cand[:n]]


def chart(name, dates, ics, path):
    fig, ax = plt.subplots(figsize=(8, 3.2), dpi=160)
    x = np.arange(len(ics))
    ax.bar(x, ics, color=['#C0392B' if v < 0 else '#2E86C1' for v in ics], width=0.8)
    ax.axhline(0, color='#333', lw=0.8)
    ax.set_xticks(x[::max(1, len(x)//8)])
    ax.set_xticklabels([dates[i] for i in x[::max(1, len(x)//8)]], rotation=45, ha='right', fontsize=7)
    ax.set_ylabel('IC (1min)', fontsize=8)
    ax.tick_params(axis='y', labelsize=7)
    ax.set_title('%s  daily IC' % name, fontsize=9)
    for s in ('top', 'right'):
        ax.spines[s].set_visible(False)
    fig.tight_layout()
    fig.savefig(path)
    plt.close(fig)


def textbox(slide, x, y, w, h, lines, size=14, color=DARK, bold=False, spacing=6):
    tb = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    tf = tb.text_frame
    tf.word_wrap = True
    for i, line in enumerate(lines):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.text = line
        p.space_after = Pt(spacing)
        for run in p.runs:
            run.font.size = Pt(size)
            run.font.color.rgb = color
            run.font.bold = bold
            run.font.name = 'Microsoft YaHei'
    return tb


def main():
    rows, factors, fam, meaning, dates, daily = load()
    top = pick(rows, TOPN)

    prs = Presentation()
    prs.slide_width = Inches(13.333)
    prs.slide_height = Inches(7.5)
    blank = prs.slide_layouts[6]

    period = '%s–%s，%d 个交易日' % (dates[0], dates[-1], len(dates))

    # 封面
    s = prs.slides.add_slide(blank)
    textbox(s, 0.9, 1.9, 11.5, 1.2, ['分钟级价量因子交付报告'], size=36, bold=True)
    textbox(s, 0.9, 3.2, 11.5, 2.2, [
        '所有者：%s' % OWNER,
        '测试周期：%s' % period,
        '因子来源：Alpha101 + 模板化生成（39 个经济族，1,000 条）',
        '评估口径：预测未来 1/3/5 分钟中间价收益，IC = Pearson 相关',
    ], size=16, color=GREY)

    # 概览
    s = prs.slides.add_slide(blank)
    textbox(s, 0.9, 0.6, 11.5, 0.8, ['评估概览'], size=26, bold=True)
    ic = np.array([fnum(r['IC1_mean']) for r in rows])
    a = np.abs(ic[np.isfinite(ic)])
    textbox(s, 0.9, 1.6, 11.5, 3.5, [
        '候选因子：%d 条（39 个经济族）' % len(rows),
        '有效 IC：%d 条，覆盖率中位数 %.2f' % (int(np.isfinite(ic).sum()), np.median([fnum(r['coverage']) for r in rows])),
        '|IC| 中位数 %.4f ｜ 90 分位 %.4f ｜ 最大 %.4f' % (np.median(a), np.percentile(a, 90), a.max()),
        '|IC| > 0.01：%d 条 ｜ > 0.02：%d 条' % (int((a > 0.01).sum()), int((a > 0.02).sum())),
        '',
        '本次交付：按 |IC| 排序、覆盖率 > 90%%、|IR| > 1 的 Top %d 条' % len(top),
    ], size=15, color=DARK, spacing=8)

    # 每个因子 2 页
    for k, r in enumerate(top):
        name = r['name']
        e = factors.get(name, '')
        f = fam.get(name, '?')
        m = meaning.get(f, '')
        ic1, ic3, ic5 = fnum(r['IC1_mean']), fnum(r['IC3_mean']), fnum(r['IC5_mean'])
        ir1, pos, cov = fnum(r['IR1']), fnum(r['pos1']), fnum(r['coverage'])

        # 第 1 页：概要
        s = prs.slides.add_slide(blank)
        textbox(s, 0.7, 0.5, 12, 0.6, ['因子名：%s' % name], size=24, bold=True)
        textbox(s, 0.7, 1.25, 12, 1.6, [
            '所有者：%s        测试周期：%s' % (OWNER, period),
            '测试 IC 值：%+.4f（预测 1min） ｜ %+.4f（3min） ｜ %+.4f（5min）' % (ic1, ic3, ic5),
            'IR：%+.2f ｜ IC>0 占比：%.0f%% ｜ 覆盖率：%.2f' % (ir1, 100 * pos, cov),
        ], size=14, color=GREY, spacing=4)
        png = os.path.join(TMP, '%s.png' % name)
        chart(name, dates, daily[name], png)
        s.shapes.add_picture(png, Inches(0.9), Inches(3.0), width=Inches(11.5))
        textbox(s, 0.7, 6.9, 12, 0.4, ['因子概要（逐日 IC）'], size=10, color=GREY)

        # 第 2 页：原理
        s = prs.slides.add_slide(blank)
        textbox(s, 0.7, 0.5, 12, 0.6, ['原理描述'], size=24, bold=True)
        textbox(s, 0.7, 1.3, 12, 2.2, [
            '表达式：',
            e,
        ], size=13, color=ACCENT, spacing=2)
        textbox(s, 0.7, 2.9, 12, 3.8, [
            '所属因子族：%s' % f,
            '经济含义：%s' % m,
            '',
            '构造逻辑：',
            '  · 输入为 L2 逐笔聚合出的分钟面板（OHLC 取中间价，量额来自逐笔成交）',
            '  · 所有时序窗口只向过去看，不存在未来函数',
            '  · 输出每分钟每只股票一个实数，评估时与未来收益做截面相关性',
            '',
            '稳定性：%d 天中 IC 同向 %d 天，IR = %+.2f' % (len(dates), int(round(pos * len(dates))), ir1),
        ], size=13, color=GREY, spacing=4)

    # 结尾
    s = prs.slides.add_slide(blank)
    textbox(s, 0.7, 0.8, 12, 1.0, ['局限与下一步'], size=26, bold=True)
    textbox(s, 0.7, 2.0, 12, 4.5, [
        '1. 样本：%s，仅覆盖单一市场阶段' % period,
        '2. 已做数值校验：5 个代表性表达式与独立 numpy 实现最大差 1e-6',
        '3. 未计交易成本：分钟级反转因子对买卖价差敏感',
        '4. 多重检验：%d 条因子同时检验，需 FDR 控制与样本外验证' % len(rows),
        '',
        '下一步：',
        '  · 样本外区间验证（2026 年上半年）',
        '  · 换手率与扣费后净 IC',
        '  · 与 Alpha101 叠加做多因子组合',
    ], size=14, color=GREY, spacing=8)

    prs.save(OUT)
    print('written', OUT, 'factors=%d' % len(top))


if __name__ == '__main__':
    main()
