#!/usr/bin/env python3
"""单月 ofi_cnt 日频截面 IC 测试（纯 numpy）。
输入：date,cidx,nB,nS,vB,vS,amtB,amtAll
输出：逐日 rank IC、均值/t 值、5 分层（均值与中位数）价差
"""
import sys
import numpy as np

DB = '/opt/bar_data'
NSTK, NDAY = 5213, 1211
MIN_N = 50
MIN_XSEC = 100

cl = np.fromfile(f'{DB}/DailyBar/clse.dat', dtype=np.float32).reshape(NDAY, NSTK).astype(np.float64)
ad = np.fromfile(f'{DB}/DailyBar/adj.dat', dtype=np.float32).reshape(NDAY, NSTK).astype(np.float64)
days = [l.strip()[:8] for l in open(f'{DB}/Meta/daily_time_list.txt')]
cl[cl <= 0] = np.nan
ad[~np.isfinite(ad) | (ad <= 0)] = 1.0
dayidx = {d: i for i, d in enumerate(days)}


def rank01(a):
    o = np.argsort(a, kind='mergesort')
    r = np.empty(len(a), dtype=np.float64)
    r[o] = np.arange(len(a), dtype=np.float64)
    return r / max(len(a) - 1, 1)


fn = sys.argv[1]
rows = []
for ln in open(fn):
    p = ln.strip().split(',')
    if len(p) < 7 or not p[0].isdigit():
        continue
    rows.append(p)
a = np.array(rows, dtype=np.float64)
print(f'原始行数 {len(a):,}')
date_i = a[:, 0].astype(np.int64)
cidx = a[:, 1].astype(np.int64)
nB, nS, vB, vS, amtB, amtAll = a[:, 2:].T

tn = nB + nS
keep = (tn >= MIN_N) & (cidx < NSTK) & (cidx >= 0)
date_i, cidx, nB, nS = date_i[keep], cidx[keep], nB[keep], nS[keep]
tn = tn[keep]
fac = (nB - nS) / tn
print(f'过滤后 {len(fac):,} 行，交易日 {len(np.unique(date_i))}')

di = np.array([dayidx.get(str(int(d)), -1) for d in date_i], dtype=np.int64)
ok = (di >= 0) & (di + 1 < NDAY)
di, cidx, fac = di[ok], cidx[ok], fac[ok]
p0 = cl[di, cidx] * ad[di, cidx]
r1 = cl[di + 1, cidx] * ad[di + 1, cidx] / p0 - 1.0
good = np.isfinite(r1) & np.isfinite(fac)
lim = np.nanquantile(r1[np.isfinite(r1)], [0.001, 0.999])
r1 = np.clip(r1, *lim)

order = np.argsort(di, kind='mergesort')
di_s, fac, r1 = di[order], fac[order], r1[order]
bounds = np.flatnonzero(np.r_[True, di_s[1:] != di_s[:-1]])
starts = bounds
ends = np.r_[bounds[1:], len(di_s)]

ics, qtiles = [], []
for s, e in zip(starts, ends):
    xs, ys = fac[s:e], r1[s:e]
    m = np.isfinite(xs) & np.isfinite(ys)
    if m.sum() < MIN_XSEC:
        continue
    xr, yr = rank01(xs[m]), rank01(ys[m])
    ics.append(np.corrcoef(xr, yr)[0, 1])
    b = np.minimum((xr * 5).astype(int), 4)
    ys_ = ys[m]
    lo, hi = ys_[b == 0], ys_[b == 4]
    qtiles.append((np.mean(hi) - np.mean(lo), np.median(hi) - np.median(lo)))

ics = np.array(ics)
qt = np.array(qtiles)
n = len(ics)
print(f'\n=== ofi_cnt 单月 IC（{n} 个交易日）===')
print(f'IC 均值   {ics.mean():.4f}')
print(f'IC 标准差 {ics.std(ddof=1):.4f}')
print(f'IC t 值   {ics.mean() / (ics.std(ddof=1) / np.sqrt(n)):.2f}')
print(f'IC>0 占比 {np.mean(ics > 0):.1%}')
print(f'分层 Q5-Q1 均值价差   {qt[:, 0].mean()*100:.2f} bp')
print(f'分层 Q5-Q1 中位价差   {qt[:, 1].mean()*100:.2f} bp')
print('\n逐日 IC:')
for d, ic in zip(di_s[starts], ics):
    print(f'  {days[d]}  {ic:+.4f}')
