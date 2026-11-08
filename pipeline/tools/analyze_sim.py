# -*- coding: utf-8 -*-
"""汇总 batch_sim.sh 的逐日结果。"""
import re, sys, os, glob, statistics as st

d = sys.argv[1] if len(sys.argv) > 1 else 'out_sim'
rows = []
for log in sorted(glob.glob(os.path.join(d, '*.log'))):
    date = os.path.basename(log)[:-4]
    txt = open(log, errors='ignore').read()
    def g(pat, cast=float):
        m = re.search(pat, txt)
        return cast(m.group(1)) if m else None
    eq = g(r'equity_final=([\d.\-]+)')
    ret = g(r'ret=([\d.\-]+)%')
    fee = g(r'fees=([\d.\-]+)')
    cash = g(r'cash=([\d.\-]+)')
    tr = g(r'trades=(\d+)', int)
    if eq is None: continue
    rows.append(dict(date=date, ret=ret, fee=fee, cash=cash, trades=tr, eq=eq))

if not rows:
    print('no results'); sys.exit(0)

rows.sort(key=lambda r: r['date'])
print(f"{'date':10s} {'ret%':>7s} {'fee%':>6s} {'cash%':>6s} {'trades':>7s}")
for r in rows:
    fee_pct = r['fee']/1e6*100
    cash_pct = r['cash']/r['eq']*100 if r['eq'] else 0
    print(f"{r['date']:10s} {r['ret']:+7.3f} {fee_pct:6.3f} {cash_pct:6.1f} {r['trades']:7d}")

rets = [r['ret'] for r in rows]
n = len(rets)
mean = st.mean(rets); sd = st.stdev(rets) if n > 1 else 0
win = sum(1 for x in rets if x > 0) / n
print(f"\nn={n}  mean={mean:+.3f}%/day  std={sd:.3f}  sharpe={mean/sd*(244**0.5) if sd else 0:.2f}")
print(f"win={win:.0%}  best={max(rets):+.2f}%  worst={min(rets):+.2f}%  cum={((1+mean/100)**n-1)*100:+.1f}%")
