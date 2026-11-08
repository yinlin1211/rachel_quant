#!/usr/bin/env python3
"""订单流因子 IC + 分层回测（纯 numpy，环境里 pandas 导入会卡死）
数据源：ClickHouse od 表聚合出的 raw_all.csv
"""
import numpy as np, sys, os, glob

BASE = '/home/intern2/rachel/ofz'
DB = '/opt/bar_data'
NSTK, NDAY = 5213, 1211
MIN_N = 50          # 当天委托笔数下限（流动性过滤）
MIN_XSEC = 100      # 有效截面股票数下限


def load_daily():
    cl = np.fromfile(f'{DB}/DailyBar/clse.dat', dtype=np.float32).reshape(NDAY, NSTK).astype(np.float64)
    ad = np.fromfile(f'{DB}/DailyBar/adj.dat', dtype=np.float32).reshape(NDAY, NSTK).astype(np.float64)
    days = [l.strip()[:8] for l in open(f'{DB}/Meta/daily_time_list.txt')]
    cl[cl <= 0] = np.nan
    ad[~np.isfinite(ad) | (ad <= 0)] = 1.0
    return cl, ad, days


def rank01(a):
    o = np.argsort(a, kind='mergesort')
    r = np.empty(len(a), dtype=np.float64)
    r[o] = np.arange(len(a), dtype=np.float64)
    return r / max(len(a) - 1, 1)


def load_raw():
    files = sorted(f for f in glob.glob(f'{BASE}/raw_*.csv') if not f.endswith('raw_all.csv'))
    print('读取', [os.path.basename(f) for f in files])
    rows = []
    for f in files:
        with open(f) as fh:
            for ln in fh:
                p = ln.split(',')
                if len(p) < 15:
                    continue
                rows.append(p[:15])
    arr = np.array(rows, dtype=np.float64)
    print(f'原始行数 {len(arr):,}')
    return arr


def main():
    cl, ad, days = load_daily()
    dayidx = {d: i for i, d in enumerate(days)}
    tkr = [l.strip() for l in open(f'{DB}/Meta/tkr.txt')]
    is_sh = np.array([t.endswith('.SH') for t in tkr], dtype=bool)

    a = load_raw()
    date_i = a[:, 0].astype(np.int64)
    cidx = a[:, 1].astype(np.int64)
    nB, nS, vB, vS, nD, nDB, nDS, vD, nBig, vBig, vLate, amtB, amtAll = a[:, 2:].T
    del a

    tn, tv = nB + nS, vB + vS
    keep = (tn >= MIN_N) & (tv > 0) & (cidx < NSTK)
    date_i, cidx, nB, nS, vB, vS, nD, nDB, nDS, vD, nBig, vBig, vLate, amtB, amtAll, tn, tv = [
        x[keep] for x in (date_i, cidx, nB, nS, vB, vS, nD, nDB, nDS, vD, nBig, vBig, vLate, amtB, amtAll, tn, tv)]

    with np.errstate(divide='ignore', invalid='ignore'):
        F = {
            'ofi_cnt':    (nB - nS) / tn,
            'ofi_vol':    (vB - vS) / tv,
            'avg_size':   np.log1p(tv / tn),
            'big_ratio':  vBig / tv,
            'late_ratio': vLate / tv,
            'px_pos':     (amtB / vB) / (amtAll / tv) - 1.0,
        }
        sh = is_sh[cidx]
        F['cancel_rate'] = np.where(sh, nD / (tn + nD), np.nan)
        F['cancel_imb'] = np.where(sh, (nDB - nDS) / (nDB + nDS), np.nan)
    names = list(F)

    # 日期 -> 日历下标
    di = np.array([dayidx.get(str(d), -1) for d in date_i], dtype=np.int64)
    ok = (di >= 0) & (di + 5 < NDAY)
    di, cidx = di[ok], cidx[ok]
    for k in F:
        F[k] = F[k][ok]
    date_i = date_i[ok]
    print(f'可用行数 {len(di):,}  涉及交易日 {len(np.unique(di))}')

    p0 = cl[di, cidx] * ad[di, cidx]
    r1 = cl[di + 1, cidx] * ad[di + 1, cidx] / p0 - 1.0
    r5 = cl[di + 5, cidx] * ad[di + 5, cidx] / p0 - 1.0
    good = np.isfinite(r1)
    for q in (0.001, 0.999):
        pass
    lim1 = np.nanquantile(r1[good], [0.001, 0.999])
    lim5 = np.nanquantile(r5[np.isfinite(r5)], [0.001, 0.999])
    r1 = np.clip(r1, *lim1)
    r5 = np.clip(r5, *lim5)

    # 按日期分组
    order = np.argsort(di, kind='mergesort')
    di_s = di[order]
    bounds = np.flatnonzero(np.r_[True, di_s[1:] != di_s[:-1]])
    starts = bounds
    ends = np.r_[bounds[1:], len(di_s)]
    print(f'交易日 {len(starts)}')

    results, bts = [], {}
    for fac in names:
        x_all = F[fac][order]
        for hname, ret in (('ret1', r1), ('ret5', r5)):
            y_all = ret[order]
            ics, bucket_sum, npair = [], np.zeros(5), 0
            for s, e in zip(starts, ends):
                xs, ys = x_all[s:e], y_all[s:e]
                m = np.isfinite(xs) & np.isfinite(ys)
                if m.sum() < MIN_XSEC:
                    continue
                xr = rank01(xs[m]); yr = rank01(ys[m])
                ics.append(np.corrcoef(xr, yr)[0, 1])
                if hname == 'ret1':
                    b = np.minimum((xr * 5).astype(int), 4)
                    for k in range(5):
                        bucket_sum[k] += ys[m][b == k].mean()
                npair += m.sum()
            ics = np.array([v for v in ics if np.isfinite(v)])
            if len(ics) < 10:
                continue
            results.append(dict(factor=fac, horizon=hname, days=len(ics), ic=ics.mean(),
                                ic_std=ics.std(ddof=1), icir=ics.mean() / ics.std(ddof=1),
                                t=ics.mean() / ics.std(ddof=1) * np.sqrt(len(ics)),
                                pos=float((ics > 0).mean()), avg_stocks=npair / len(ics)))
            if hname == 'ret1':
                bts[fac] = bucket_sum / len(ics)

    print('\n===== IC（Spearman）=====')
    hdr = f"{'factor':<12}{'hor':<6}{'days':>6}{'IC':>9}{'ICstd':>9}{'ICIR':>8}{'t':>8}{'IC>0':>8}{'股票数':>8}"
    print(hdr); print('-' * len(hdr))
    for r in results:
        print(f"{r['factor']:<12}{r['horizon']:<6}{r['days']:>6}{r['ic']:>9.4f}{r['ic_std']:>9.4f}"
              f"{r['icir']:>8.3f}{r['t']:>8.2f}{r['pos']:>8.1%}{r['avg_stocks']:>8.0f}")
    with open(f'{BASE}/ic_summary.csv', 'w') as fh:
        fh.write('factor,horizon,days,ic,ic_std,icir,t,pos,avg_stocks\n')
        for r in results:
            fh.write(','.join(str(r[k]) for k in ['factor', 'horizon', 'days', 'ic', 'ic_std', 'icir', 't', 'pos', 'avg_stocks']) + '\n')

    # ---- 五分层（次日收益，等权）----
    print('\n===== 五分层日均收益（bp，1 日持有，等权）=====')
    hdr2 = f"{'factor':<12}" + ''.join(f"{'Q'+str(i+1):>9}" for i in range(5)) + f"{'Q5-Q1':>10}{'年化':>10}{'Sharpe':>9}"
    print(hdr2); print('-' * len(hdr2))
    bt_rows = []
    for fac, b in bts.items():
        q = b * 1e4
        ls = b[4] - b[0]
        ann = (1 + ls) ** 244 - 1
        # Sharpe 需要日序列，这里用分层均值近似（下一段用日序列重算）
        bt_rows.append((fac, q, ls, ann))
        print(f"{fac:<12}" + ''.join(f"{v:>9.2f}" for v in q) + f"{ls*1e4:>10.2f}{ann:>10.2%}{'':>9}")

    print('\n各因子换手率与成本敏感性：')
    for fac in names:
        x_all = F[fac][order]; y_all = r1[order]
        prev_hi = prev_lo = None
        to_hi, to_lo, sig = [], [], []
        for s, e in zip(starts, ends):
            xs, ys = x_all[s:e], y_all[s:e]
            m = np.isfinite(xs) & np.isfinite(ys)
            if m.sum() < MIN_XSEC:
                continue
            cs = cidx[order][s:e][m]
            xr = rank01(xs[m])
            hi = set(cs[xr >= 0.8].tolist()); lo = set(cs[xr < 0.2].tolist())
            if prev_hi is not None:
                to_hi.append(1 - len(hi & prev_hi) / max(len(hi), 1))
                to_lo.append(1 - len(lo & prev_lo) / max(len(lo), 1))
            prev_hi, prev_lo = hi, lo
            sig.append(ys[m][xr >= 0.8].mean() - ys[m][xr < 0.2].mean())
        sig = np.array(sig); th = np.mean(to_hi); tl = np.mean(to_lo)
        gross = sig.mean() * 1e4
        print(f"  {fac:<12} 毛价差 {gross:6.2f}bp  多头换手 {th:5.1%}  空头换手 {tl:5.1%}")
        for bp in (5, 10, 20):
            net = gross - (th + tl) * bp
            print(f"     单边 {bp:>2}bp: 净价差 {net:6.2f}bp  年化 {((1+net/1e4)**244-1):>7.1%}")

    print('\n各因子 Q5-Q1 的日序列统计：')
    for fac in names:
        x_all = F[fac][order]; y_all = r1[order]
        sig, ret = [], []
        for s, e in zip(starts, ends):
            xs, ys = x_all[s:e], y_all[s:e]
            m = np.isfinite(xs) & np.isfinite(ys)
            if m.sum() < MIN_XSEC:
                continue
            xr = rank01(xs[m]); b = np.minimum((xr * 5).astype(int), 4)
            hi = ys[m][b == 4].mean(); lo = ys[m][b == 0].mean()
            sig.append(hi - lo); ret.append(hi)
        sig = np.array(sig)
        if len(sig) < 10:
            continue
        nav = np.cumprod(1 + sig)
        sharpe = sig.mean() / sig.std(ddof=1) * np.sqrt(244)
        mdd = (nav / np.maximum.accumulate(nav) - 1).min()
        print(f"  {fac:<12} 年化 {((1+sig.mean())**244-1):>8.2%}  Sharpe {sharpe:>5.2f}  "
              f"最大回撤 {mdd:>7.2%}  日胜率 {(sig>0).mean():>5.1%}  天数 {len(sig)}")


if __name__ == '__main__':
    main()
