# -*- coding: utf-8 -*-
"""从 ic_stats.csv 汇总 IC（最终结果，不读 .dat）。

ic_stats.csv 由 alpha101 在内存中直接累加得到，每行是
(date, factor, horizon, n, Σx, Σy, Σxy, Σx², Σy²)，
由此可精确重建逐日 IC 与合并 IC。

用法:
    python3 summarize_ic.py <out_dir> [horizon=1]
"""
import os
import sys
import numpy as np

OUT = sys.argv[1] if len(sys.argv) > 1 else 'out_10d'
HZ = int(sys.argv[2]) if len(sys.argv) > 2 else 1


def load_info(path):
    with open(os.path.join(path, 'alphaInfo.txt')) as f:
        head = f.readline().strip().split(',')
        nstk, nalpha = int(head[0]), int(head[1])
        names = [f.readline().strip() for _ in range(nalpha)]
    return nstk, nalpha, names


def ic_from_stats(n, sx, sy, sxy, sxx, syy):
    cov = n * sxy - sx * sy
    vx = n * sxx - sx * sx
    vy = n * syy - sy * sy
    den = np.sqrt(np.maximum(vx, 0) * np.maximum(vy, 0))
    with np.errstate(invalid='ignore', divide='ignore'):
        ic = np.where((den > 0) & (n > 10), cov / den, np.nan)
    return ic


def main():
    nstk, nalpha, names = load_info(OUT)
    path = os.path.join(OUT, 'ic_stats.csv')
    raw = np.genfromtxt(path, delimiter=',', names=True, dtype=None,
                        encoding='utf-8')
    raw = np.atleast_1d(raw)
    sel = raw[raw['horizon'] == HZ]

    days = sorted(set(int(x) for x in sel['date']))
    nd = len(days)
    day_idx = {d: i for i, d in enumerate(days)}
    fac_idx = {n: i for i, n in enumerate(names)}

    N = np.zeros((nalpha, nd)); SX = np.zeros((nalpha, nd)); SY = np.zeros((nalpha, nd))
    SXY = np.zeros((nalpha, nd)); SXX = np.zeros((nalpha, nd)); SYY = np.zeros((nalpha, nd))
    for r in sel:
        k = fac_idx[str(r['factor'])]
        d = day_idx[int(r['date'])]
        N[k, d] = r['n']; SX[k, d] = r['sx']; SY[k, d] = r['sy']
        SXY[k, d] = r['sxy']; SXX[k, d] = r['sxx']; SYY[k, d] = r['syy']

    per_day = ic_from_stats(N, SX, SY, SXY, SXX, SYY)
    pooled = ic_from_stats(N.sum(axis=1), SX.sum(axis=1), SY.sum(axis=1),
                           SXY.sum(axis=1), SXX.sum(axis=1), SYY.sum(axis=1))
    cover = N.sum(axis=1) / float(240 * nstk * nd)

    rows = []
    for k in range(nalpha):
        ics = per_day[k]
        v = np.isfinite(ics)
        mean_ic = np.nanmean(ics) if v.any() else np.nan
        std_ic = np.nanstd(ics) if v.any() else np.nan
        ir = mean_ic / std_ic if (v.any() and std_ic > 0) else np.nan
        pos = float(np.mean(ics[v] > 0)) if v.any() else np.nan
        rows.append((names[k], mean_ic, std_ic, ir, pos, pooled[k], cover[k]))

    rows.sort(key=lambda r: -abs(r[1]) if np.isfinite(r[1]) else 0)
    out = os.path.join(OUT, 'result_alpha101.csv')
    with open(out, 'w') as f:
        f.write('name,IC_mean,IC_std,IR,IC_pos_ratio,IC_pooled,coverage\n')
        for r in rows:
            f.write('%s,%.6f,%.6f,%.4f,%.4f,%.6f,%.4f\n' % r)

    print('horizon=%dmin days=%d factors=%d' % (HZ, nd, nalpha))
    print('written', out)
    print('%-10s %10s %10s %8s %8s %10s %8s' % ('name', 'IC_mean', 'IC_std', 'IR', 'pos', 'IC_pooled', 'cover'))
    for r in rows[:15]:
        print('%-10s %10.5f %10.5f %8.3f %8.2f %10.5f %8.2f' % (r[0], r[1], r[2], r[3], r[4], r[5], r[6]))


if __name__ == '__main__':
    main()
