# -*- coding: utf-8 -*-
"""用独立的 numpy 实现校验 C++ 表达式求值（数值正确性校验）。

只覆盖 5 个代表性表达式，覆盖：逐元素、ts_rank、ts_corr+rank、ts_mean、decay/delay。
两边用同一份面板、同一天的划分，比较 IC。

用法：python3 validate_expr.py <panel 目录> <日期...>
"""
import sys
import os
import numpy as np
import pandas as pd

PANEL = sys.argv[1]
DATES = sys.argv[2:]

DT = np.dtype([('nTime','<i4'),('minute','<i4'),('cidx','<i4'),('nMd','<i4'),('nTs','<i4'),
               ('open','<f4'),('high','<f4'),('low','<f4'),('close','<f4'),
               ('vwap','<f4'),('volume','<f4'),('amount','<f4')])

EXPRS = {
    'v1': "div(sub(close,open), add(sub(high,low),0.001))",
    'v2': "ts_rank(close,10)",
    'v3': "ts_corr(rank(volume), rank(close), 20)",
    'v4': "sub(div(close, ts_mean(close,10)),1)",
    'v5': "decay(sub(div(close,delay(close,1)),1), 5)",
}


def rank_cs(x):
    t, n = x.shape
    r = np.full((t, n), np.nan)
    for i in range(t):
        row = x[i]
        m = np.isfinite(row)
        if m.sum() == 0:
            continue
        v = row[m]
        cnt = len(v)
        order = np.argsort(v, kind='mergesort')
        ranks = np.empty(cnt)
        ranks[order] = np.arange(cnt)
        sv = v[order]
        a = 0
        while a < cnt:
            b = a
            while b + 1 < cnt and sv[b + 1] == sv[a]:
                b += 1
            if b > a:
                ranks[order[a:b + 1]] = 0.5 * (a + b)
            a = b + 1
        r[i, m] = ranks / (cnt - 1) if cnt > 1 else 0.5
    return r


def ts_rank(a, w):
    df = pd.DataFrame(a)
    r = df.rolling(w, min_periods=w).apply(lambda x: (x <= x[-1]).sum() / len(x), raw=True).values
    return r


def ts_corr(a, b, w):
    da, db = pd.DataFrame(a), pd.DataFrame(b)
    out = da.rolling(w, min_periods=w).corr(db).values
    # C++ 要求窗口内全部有效，否则 NaN
    nan_a = da.isna().rolling(w, min_periods=1).sum().values
    nan_b = db.isna().rolling(w, min_periods=1).sum().values
    out[(nan_a > 0) | (nan_b > 0)] = np.nan
    return out


def ts_mean(a, w):
    return pd.DataFrame(a).rolling(w, min_periods=w).mean().values


def decay(a, w):
    weights = np.arange(1, w + 1, dtype=float)   # 窗口由老到新，最近权重最大
    weights /= weights.sum()
    return pd.DataFrame(a).rolling(w, min_periods=w).apply(
        lambda x: np.dot(x, weights) if np.isfinite(x).all() else np.nan, raw=True).values


def delay(a, d):
    out = np.full_like(a, np.nan)
    out[d:] = a[:-d]
    return out


def load():
    fields = {k: [] for k in ('open','high','low','close','vwap','volume','amount')}
    for d in DATES:
        a = np.fromfile(os.path.join(PANEL, d + '.panel'), dtype=DT)
        n = a.shape[0] // 240
        a = a.reshape(240, n)
        for k in fields:
            fields[k].append(a[k].astype(np.float64))
    return {k: np.vstack(v) for k, v in fields.items()}, 240 * len(DATES)


def ic_by_day(x, y, ndays):
    out = []
    for d in range(ndays):
        xs = x[d*240:(d+1)*240].ravel()
        ys = y[d*240:(d+1)*240].ravel()
        m = np.isfinite(xs) & np.isfinite(ys)
        if m.sum() < 10:
            out.append(np.nan); continue
        xv, yv = xs[m] - xs[m].mean(), ys[m] - ys[m].mean()
        den = np.sqrt((xv*xv).sum() * (yv*yv).sum())
        out.append((xv*yv).sum()/den if den > 0 else np.nan)
    return np.array(out)


def main():
    f, T = load()
    open_, high, low, close, vwap, volume = (f['open'], f['high'], f['low'],
                                             f['close'], f['vwap'], f['volume'])
    # 未来 1 分钟收益（按天截断）
    ret = np.full_like(close, np.nan)
    for d in range(len(DATES)):
        s = d*240
        ret[s:s+239] = (close[s+1:s+240] / close[s:s+239] - 1.0) * 10000.0

    vals = {}
    vals['v1'] = (close - open_) / (high - low + 0.001)
    vals['v2'] = ts_rank(close, 10)
    vals['v3'] = ts_corr(rank_cs(volume), rank_cs(close), 20)
    vals['v4'] = close / ts_mean(close, 10) - 1.0
    vals['v5'] = decay(close / delay(close, 1) - 1.0, 5)

    print('%-4s %12s %12s %12s' % ('id', 'python IC', 'C++ IC', 'diff'))
    for k, v in vals.items():
        ics = ic_by_day(v, ret, len(DATES))
        print('%-4s %12.6f' % (k, np.nanmean(ics)))

    with open('/tmp/validate_factors.txt', 'w') as fo:
        for k, e in EXPRS.items():
            fo.write('%s|%s\n' % (k, e))


if __name__ == '__main__':
    main()
