#!/usr/bin/env python3
"""Analyze panelbook: spread/depth/imbalance and event-day context."""
import struct, sys, os

FIELDS = (
    'nTime','minute','cidx','nMd','nTs','open','high','low','close','vwap',
    'volume','amount','bid1','ask1','bidvol1','askvol1','totbid','totask',
    'buyvol','sellvol','ofi','limitUp','limitDown'
)
FMT = '<5i18f'
ROW_SIZE = struct.calcsize(FMT)
NSLOT = 240

def rows(path):
    size = os.path.getsize(path)
    n = size // ROW_SIZE
    if n * ROW_SIZE != size:
        raise ValueError('bad panelbook size: %d' % (n,))
    with open(path, 'rb') as f:
        raw = f.read(ROW_SIZE)
        for i in range(n):
            yield dict(zip(FIELDS, struct.unpack(FMT, raw)))
            raw = f.read(ROW_SIZE)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'pipeline/panel_books/20260105.panelbook'
    topN = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    rows_iter = rows(path)
    spread, depth, spread_bps, depth_total, ofi, amt = ([] for _ in range(6))
    buyvol, sellvol, limit_up, limit_down = ([] for _ in range(4))
    min_time = None
    max_time = None
    bid_only = 0
    ask_only = 0
    both = 0
    neither = 0
    n = 0
    for row in rows_iter:
        if min_time is None or row['nTime'] < min_time:
            min_time = row['nTime']
        if max_time is None or row['nTime'] > max_time:
            max_time = row['nTime']
        mid = 0.5 * (row['bid1'] + row['ask1'])
        if row['bid1'] > 0 and row['ask1'] > 0:
            both += 1
            if mid > 0:
                sp = row['ask1'] - row['bid1']
                spread.append(sp)
                spread_bps.append(1e4 * sp / mid)
        elif row['bid1'] > 0:
            bid_only += 1
        elif row['ask1'] > 0:
            ask_only += 1
        else:
            neither += 1
        if row['bidvol1'] > 0 or row['askvol1'] > 0:
            depth.append(row['bidvol1'] + row['askvol1'])
        if row['totbid'] > 0 or row['totask'] > 0:
            depth_total.append(row['totbid'] + row['totask'])
        if isinstance(row['ofi'], (int, float)):
            ofi.append(abs(row['ofi']))
        amt.append(abs(row['amount']))
        buyvol.append(abs(row['buyvol']))
        sellvol.append(abs(row['sellvol']))
        if row['limitUp'] > 0:
            limit_up.append(row['limitUp'])
        if row['limitDown'] > 0:
            limit_down.append(row['limitDown'])
        n += 1

    def pct(xs, q):
        xs = sorted(xs)
        if not xs:
            return None
        k = int((len(xs) - 1) * q)
        return xs[k]

    group = dict(
        spread_bp = spread_bps,
        depth_l1 = depth,
        depth_total = depth_total,
        abs_ofi = ofi,
        amount = amt,
        buyvol = buyvol,
        sellvol = sellvol,
    )
    print('path', path)
    print('time', min_time, max_time, 'topN', topN)
    print('rows', n)
    print('one_sided', bid_only, ask_only, 'both', both, 'neither', neither)
    for name, values in group.items():
        if not values:
            continue
        print(name, 'count', len(values), 'mean', sum(values) / len(values))
        for q in (0.05, 0.25, 0.5, 0.75, 0.95):
            print(name, 'p%02d' % int(q * 100), pct(values, q))
    print('limitUp rows', len(limit_up), 'limitDown rows', len(limit_down))
    print('top volume rows')
    secondary = sorted(rows(path), key=lambda r: -abs(r['amount']))[:topN]
    for row in secondary[:5]:
        print('amount_top', row['minute'], row['cidx'], row['amount'], row['volume'])
    heavy = sorted(rows(path), key=lambda r: -(r['totbid'] + r['totask']))[:topN]
    for row in heavy[:5]:
        print('depth_top', row['minute'], row['cidx'], row['totbid'], row['totask'])


if __name__ == '__main__':
    main()
