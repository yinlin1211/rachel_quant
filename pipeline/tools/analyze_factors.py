# -*- coding: utf-8 -*-
"""对 alpha_expr 的输出做完整评估。

输入：
  <summary.csv>   汇总（多周期 IC）
  <daily.csv>     逐日 IC（1 分钟）
  <map.csv>       id -> family（可选）
  <catalog.csv>   家族含义（可选）

输出（打印 + 写 <out_dir>/evaluation.md）：
  1. 分布与筛选
  2. 前 N 名（含逐日稳定性）
  3. 前后半段样本外检验
  4. 周期衰减（1/3/5 分钟）
  5. 家族汇总
  6. 按逐日 IC 相关性去重
"""
import sys
import os
import csv
import numpy as np
from collections import defaultdict

SUMMARY = sys.argv[1]
DAILY = sys.argv[2]
MAPF = sys.argv[3] if len(sys.argv) > 3 else None
CATF = sys.argv[4] if len(sys.argv) > 4 else None
OUTDIR = sys.argv[5] if len(sys.argv) > 5 else os.path.dirname(SUMMARY) or '.'
TOPN = 20


def fnum(x):
    try:
        v = float(x)
        return v
    except Exception:
        return np.nan


def load_summary():
    rows = list(csv.DictReader(open(SUMMARY)))
    out = []
    for r in rows:
        out.append(dict(
            name=r['name'], expr=r['expr'],
            ic=fnum(r.get('IC1_mean')), std=fnum(r.get('IC1_std')),
            ir=fnum(r.get('IR1')), pos=fnum(r.get('pos1')),
            pooled=fnum(r.get('IC1_pooled')), cov=fnum(r.get('coverage')),
            ic3=fnum(r.get('IC3_mean')), ic5=fnum(r.get('IC5_mean')),
        ))
    return out


def load_daily():
    rows = list(csv.reader(open(DAILY)))
    dates = rows[0][1:]
    data = {}
    for r in rows[1:]:
        data[r[0]] = np.array([fnum(x) for x in r[1:]])
    return dates, data


def main():
    rows = load_summary()
    dates, daily = load_daily()
    fam = {}
    if MAPF and os.path.exists(MAPF):
        fam = {r['id']: r['family'] for r in csv.DictReader(open(MAPF))}
    meaning = {}
    if CATF and os.path.exists(CATF):
        meaning = {r['family']: r['meaning'] for r in csv.DictReader(open(CATF))}

    ic = np.array([r['ic'] for r in rows])
    cov = np.array([r['cov'] for r in rows])
    valid = np.isfinite(ic)
    out = []
    P = out.append

    P('# 因子评估报告\n')
    P('样本：%d 个交易日（%s ~ %s），%d 个因子\n' % (len(dates), dates[0], dates[-1], len(rows)))

    P('## 1. 分布\n')
    a = np.abs(ic[valid])
    P('| 指标 | 数值 |\n|---|---|')
    P('| 有效 IC | %d / %d |' % (int(valid.sum()), len(rows)))
    P('| 覆盖率中位数 | %.3f |' % np.median(cov))
    for q in (50, 75, 90, 99):
        P('| \\|IC\\| %d 分位 | %.4f |' % (q, np.percentile(a, q)))
    P('| \\|IC\\| 最大 | %.4f |' % a.max())
    for th in (0.005, 0.01, 0.015, 0.02):
        P('| \\|IC\\| > %.3f | %d |' % (th, int((a > th).sum())))
    P('')

    # 2. 前 N
    order = np.argsort(-np.where(valid, np.abs(ic), 0))[:TOPN]
    P('## 2. 前 %d 名（1 分钟周期）\n' % TOPN)
    P('| 因子 | 族 | IC均值 | IC标准差 | IR | IC>0占比 | 覆盖率 |')
    P('|---|---|---|---|---|---|---|')
    for k in order:
        r = rows[k]
        P('| %s | %s | %+.4f | %.4f | %+.2f | %.0f%% | %.2f |' % (
            r['name'], fam.get(r['name'], '?'), r['ic'], r['std'], r['ir'],
            100 * r['pos'] if np.isfinite(r['pos']) else 0, r['cov']))
    P('')

    # 3. 前后半段
    half = len(dates) // 2
    P('## 3. 样本外检验（前 %d 天 vs 后 %d 天）\n' % (half, len(dates) - half))
    agree = []
    for k in order:
        nm = rows[k]['name']
        v = daily.get(nm)
        if v is None:
            continue
        h1 = np.nanmean(v[:half])
        h2 = np.nanmean(v[half:])
        same = np.sign(h1) == np.sign(h2)
        agree.append(same)
        P('| %s | %+.4f | %+.4f | %s |' % (nm, h1, h2, '✓ 同向' if same else '✗ 变号'))
    if agree:
        P('\n前 %d 名中，前后半段同向的比例：**%.0f%%**\n' % (len(agree), 100 * np.mean(agree)))

    # 4. 周期衰减
    P('## 4. 预测周期衰减（前 10 名）\n')
    P('| 因子 | IC@1min | IC@3min | IC@5min |')
    P('|---|---|---|---|')
    for k in order[:10]:
        r = rows[k]
        P('| %s | %+.4f | %+.4f | %+.4f |' % (r['name'], r['ic'], r['ic3'], r['ic5']))
    P('')

    # 5. 家族
    P('## 5. 家族汇总\n')
    g = defaultdict(list)
    for r in rows:
        if np.isfinite(r['ic']):
            g[fam.get(r['name'], '?')].append(abs(r['ic']))
    P('| 族 | 含义 | 条数 | 平均\\|IC\\| | 最大\\|IC\\| |')
    P('|---|---|---|---|---|')
    for k, v in sorted(g.items(), key=lambda kv: -np.mean(kv[1])):
        P('| %s | %s | %d | %.4f | %.4f |' % (k, meaning.get(k, '')[:28], len(v), np.mean(v), max(v)))
    P('')

    # 6. 去重（逐日 IC 序列的相关性）
    P('## 6. 前 %d 名的信号相似度\n' % TOPN)
    names = [rows[k]['name'] for k in order if rows[k]['name'] in daily]
    M = np.array([daily[n] for n in names])
    M = np.nan_to_num(M, nan=0.0)
    if len(M) > 1:
        C = np.corrcoef(M)
        pairs = []
        for i in range(len(names)):
            for j in range(i + 1, len(names)):
                pairs.append((abs(C[i, j]), names[i], names[j]))
        pairs.sort(reverse=True)
        P('| 因子对 | 逐日 IC 相关 |')
        P('|---|---|')
        for c, a1, b1 in pairs[:10]:
            P('| %s vs %s | %.3f |' % (a1, b1, c))
        P('\n相关 > 0.8 的对数：%d / %d\n' % (sum(1 for p in pairs if p[0] > 0.8), len(pairs)))

    path = os.path.join(OUTDIR, 'evaluation.md')
    with open(path, 'w') as f:
        f.write('\n'.join(out))
    print('\n'.join(out[:60]))
    print('\nwritten', path)


if __name__ == '__main__':
    main()
