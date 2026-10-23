# -*- coding: utf-8 -*-
"""Alpha101 IC 分析（按天多进程）。

.mkt 与 .dat 均为「240 分钟 × N 股票」的密集行序，收益可直接 reshape 后
沿时间轴 shift 得到，无需按股票 groupby。

用法:
    python3 analyse_a101.py <panel_dir> <out_dir> <date1,date2,...> [进程数]
"""
import sys
import os
import numpy as np
from concurrent.futures import ProcessPoolExecutor

PANEL = sys.argv[1] if len(sys.argv) > 1 else 'panels'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'out_10d'
DATES = sys.argv[3].split(',') if len(sys.argv) > 3 else [
    '20260105', '20260106', '20260107', '20260108', '20260109',
    '20260112', '20260113', '20260114', '20260115', '20260116']
NPROC = int(sys.argv[4]) if len(sys.argv) > 4 else min(len(DATES), 16)

nMin = 1  # 预测目标：1 分钟收益


def load_alpha_info(path):
    with open(os.path.join(path, 'alphaInfo.txt')) as f:
        head = f.readline().strip().split(',')
        nstk, nalpha = int(head[0]), int(head[1])
        names = [f.readline().strip() for _ in range(nalpha)]
    return nstk, nalpha, names


def process_day(job):
    """计算一天内每个因子与未来收益的 IC，以及合并所需的充分统计量。"""
    d, nstk, nalpha, nMin = job
    mkt = np.memmap(os.path.join(OUT, d + '.mkt'), dtype='int32', mode='r')
    mid = mkt.reshape(240, nstk, 4)[:, :, 3].astype(np.float64)
    alp = np.memmap(os.path.join(OUT, d + '.dat'), dtype='float32', mode='r')
    X = alp.reshape(240, nstk, nalpha)

    ret = np.full((240, nstk), np.nan)
    ret[:-nMin] = (mid[nMin:] / mid[:-nMin] - 1.0) * 10000.0
    y = ret.ravel()

    ic = np.full(nalpha, np.nan)
    cover = np.zeros(nalpha)
    n = np.zeros(nalpha)
    sx = np.zeros(nalpha); sy = np.zeros(nalpha)
    sxy = np.zeros(nalpha); sxx = np.zeros(nalpha); syy = np.zeros(nalpha)

    for k in range(nalpha):
        x = X[:, :, k].ravel()
        m = np.isfinite(x) & np.isfinite(y)
        cnt = int(m.sum())
        cover[k] = cnt / float(y.size)
        if cnt < 10:
            continue
        xv = x[m].astype(np.float64)
        yv = y[m]
        xc = xv - xv.mean()
        yc = yv - yv.mean()
        den = np.sqrt((xc * xc).sum() * (yc * yc).sum())
        ic[k] = float((xc * yc).sum() / den) if den > 0 else np.nan
        n[k] = cnt
        sx[k] = xv.sum(); sy[k] = yv.sum()
        sxy[k] = (xv * yv).sum()
        sxx[k] = (xv * xv).sum(); syy[k] = (yv * yv).sum()
    return d, ic, cover, n, sx, sy, sxy, sxx, syy


def main():
    nstk, nalpha, names = load_alpha_info(OUT)
    print('stocks=%d factors=%d days=%d procs=%d' % (nstk, nalpha, len(DATES), NPROC), flush=True)

    jobs = [(d, nstk, nalpha, nMin) for d in DATES]
    with ProcessPoolExecutor(max_workers=NPROC) as ex:
        out = list(ex.map(process_day, jobs))

    out.sort(key=lambda r: DATES.index(r[0]))
    per_day = np.vstack([r[1] for r in out])
    cover = np.vstack([r[2] for r in out])
    N = np.sum([r[3] for r in out], axis=0)
    SX = np.sum([r[4] for r in out], axis=0)
    SY = np.sum([r[5] for r in out], axis=0)
    SXY = np.sum([r[6] for r in out], axis=0)
    SXX = np.sum([r[7] for r in out], axis=0)
    SYY = np.sum([r[8] for r in out], axis=0)
    for d in DATES:
        print('%s done' % d, flush=True)

    rows = []
    for k in range(nalpha):
        ics = per_day[:, k]
        valid = np.isfinite(ics)
        mean_ic = np.nanmean(ics) if valid.any() else np.nan
        std_ic = np.nanstd(ics) if valid.any() else np.nan
        ir = mean_ic / std_ic if (valid.any() and std_ic > 0) else np.nan
        pos = np.mean(ics[valid] > 0) if valid.any() else np.nan
        pooled_ic = np.nan
        if N[k] > 10:
            nn = float(N[k])
            cov = nn * SXY[k] - SX[k] * SY[k]
            vx = nn * SXX[k] - SX[k] * SX[k]
            vy = nn * SYY[k] - SY[k] * SY[k]
            den = np.sqrt(vx * vy)
            pooled_ic = cov / den if den > 0 else np.nan
        rows.append((names[k], mean_ic, std_ic, ir, pos, pooled_ic, np.mean(cover[:, k])))

    rows.sort(key=lambda r: -abs(r[1]) if np.isfinite(r[1]) else 0)
    path = os.path.join(OUT, 'result_alpha101.csv')
    with open(path, 'w') as f:
        f.write('name,IC_mean,IC_std,IR,IC_pos_ratio,IC_pooled,coverage\n')
        for r in rows:
            f.write('%s,%.6f,%.6f,%.4f,%.4f,%.6f,%.4f\n' % r)

    print('\nwritten', path)
    print('%-10s %10s %10s %8s %8s %10s %8s' % ('name', 'IC_mean', 'IC_std', 'IR', 'pos', 'IC_pooled', 'cover'))
    for r in rows[:15]:
        print('%-10s %10.5f %10.5f %8.3f %8.2f %10.5f %8.2f' % (r[0], r[1], r[2], r[3], r[4], r[5], r[6]))


if __name__ == '__main__':
    main()
