#!/usr/bin/env python3
"""Calculate event-driven spread, depth, and executable notional from panelbooks."""
import argparse
import csv
import glob
import os
import statistics
import struct
from collections import OrderedDict


FIELDS = (
    'nTime', 'minute', 'cidx', 'nMd', 'nTs', 'open', 'high', 'low', 'close', 'vwap',
    'volume', 'amount', 'bid1', 'ask1', 'bidvol1', 'askvol1', 'totbid', 'totask',
    'buyvol', 'sellvol', 'ofi', 'limitUp', 'limitDown'
)
FMT = '<5i18f'
ROW_SIZE = struct.calcsize(FMT)
DEFAULT_HORIZONS = (1, 3, 5, 10, 15, 30, 45, 60)


def read_panelbook(path):
    size = os.path.getsize(path)
    row_count = size // ROW_SIZE
    if size % ROW_SIZE:
        raise ValueError('bad panelbook size %s: %d' % (path, size))
    with open(path, 'rb') as panel_file:
        buffer = panel_file.read(size)
        return [dict(zip(FIELDS, struct.unpack_from(FMT, buffer[offset:offset + ROW_SIZE], 0)))
                for offset in range(0, size, ROW_SIZE)]


def stock_rows(rows):
    grouped = OrderedDict()
    for row in rows:
        grouped.setdefault(row['cidx'], []).append(row)
    for candidate_rows in grouped.values():
        candidate_rows.sort(key=lambda row: row['minute'])
    return grouped


def spread_bp(row):
    bid = row['bid1']
    ask = row['ask1']
    if bid <= 0 or ask <= 0:
        return None
    mid = 0.5 * (bid + ask)
    if mid <= 0:
        return None
    return 1e4 * (ask - bid) / mid


def notional_proxy(row, side):
    price = row.get('ask1' if side == 'ask' else 'bid1', 0.0)
    volume = row.get('askvol1' if side == 'ask' else 'bidvol1', 0.0)
    if price <= 0 or volume <= 0:
        return None
    return volume * price / 1e4


def event_rows(grouped_rows):
    events = []
    for _, rows in grouped_rows.items():
        returns = [None] * len(rows)
        for index, row in enumerate(rows):
            if index < 3:
                continue
            previous = rows[index - 3]
            if previous['close'] > 0:
                returns[index] = row['close'] / previous['close'] - 1.0
        for index, row in enumerate(rows):
            if returns[index] is None:
                continue
            window = [value for value in returns[max(0, index - 19):index + 1]
                      if value is not None]
            if len(window) < 20:
                continue
            mean = sum(window) / len(window)
            variance = sum(
                (value - mean) ** 2 for value in window) / (len(window) - 1)
            standard_deviation = variance ** 0.5
            if standard_deviation <= 1e-9:
                continue
            event_z = returns[index] / standard_deviation
            if event_z < -2.0:
                events.append((index, event_z, row))
    return events


def named_stat(values):
    if not values:
        return {'mean': None, 'median': None, 'p25': None, 'p75': None}
    ordered = sorted(values)
    median = statistics.median(ordered)
    quartiles = statistics.quantiles(ordered, n=4, method='inclusive') if len(ordered) > 1 else [median, median, median]
    return {'mean': statistics.mean(values), 'median': median,
            'p25': quartiles[0], 'p75': quartiles[2]}


def event_metrics(rows, horizons, exclude_limit=False):
    grouped = stock_rows(rows)
    events = event_rows(grouped)
    if exclude_limit:
        events = [(index, event_z, row) for index, event_z, row in events
                  if not ((row['limitUp'] > 0 and row['close'] >= row['limitUp'])
                          or (row['limitDown'] > 0 and row['close'] <= row['limitDown']))]
    periods = ['same'] + ['after_%d' % horizon for horizon in horizons]
    offsets = [0] + list(horizons)
    buckets = {}
    for start_index, event_z, event_row in events:
        candidate_rows = grouped[event_row['cidx']]
        for label, offset in zip(periods, offsets):
            position = start_index + offset
            if position >= len(candidate_rows):
                continue
            row = candidate_rows[position]
            bucket = buckets.setdefault(label, {
                'spread_bp': [], 'ask_depth': [], 'bid_depth': [],
                'ask_notional': [], 'bid_notional': [], 'bid_ask_depth_ratio': [],
                'ask_l1_depth_share': [], 'bid_l1_depth_share': [],
                'ask_l1_notional_share': [], 'bid_l1_notional_share': [],
                'buy_volume': [], 'sell_volume': [], 'buy_sell_imbalance': [],
                'buy_notional': [], 'sell_notional': [],
            })
            spread = spread_bp(row)
            ask_depth = row['askvol1'] if row['askvol1'] > 0 else None
            bid_depth = row['bidvol1'] if row['bidvol1'] > 0 else None
            ask_notional = notional_proxy(row, 'ask')
            bid_notional = notional_proxy(row, 'bid')
            if spread is not None:
                bucket['spread_bp'].append(spread)
            if ask_depth is not None:
                bucket['ask_depth'].append(ask_depth)
            if bid_depth is not None:
                bucket['bid_depth'].append(bid_depth)
            if ask_notional is not None:
                bucket['ask_notional'].append(ask_notional)
            if bid_notional is not None:
                bucket['bid_notional'].append(bid_notional)
            if ask_notional is not None and row['totask'] > 0 and row['bid1'] > 0 and row['ask1'] > 0:
                mid_price = 0.5 * (row['bid1'] + row['ask1']) / 1e4
                bucket['ask_l1_notional_share'].append(ask_notional / (mid_price * row['totask']))
            if bid_notional is not None and row['totbid'] > 0 and row['bid1'] > 0 and row['ask1'] > 0:
                mid_price = 0.5 * (row['bid1'] + row['ask1']) / 1e4
                bucket['bid_l1_notional_share'].append(bid_notional / (mid_price * row['totbid']))
            if ask_depth is not None and bid_depth is not None:
                bucket['bid_ask_depth_ratio'].append(bid_depth / ask_depth)
            if ask_depth is not None and row['totask'] > 0:
                bucket['ask_l1_depth_share'].append(ask_depth / row['totask'])
            if bid_depth is not None and row['totbid'] > 0:
                bucket['bid_l1_depth_share'].append(bid_depth / row['totbid'])
            buy_volume = row['buyvol'] if row['buyvol'] > 0 else None
            sell_volume = row['sellvol'] if row['sellvol'] > 0 else None
            if buy_volume is not None:
                bucket['buy_volume'].append(buy_volume)
            if sell_volume is not None:
                bucket['sell_volume'].append(sell_volume)
            if buy_volume is not None and sell_volume is not None:
                bucket['buy_sell_imbalance'].append(
                    (buy_volume - sell_volume) / (buy_volume + sell_volume))
            vwap = row['vwap']
            if buy_volume is not None and vwap > 0:
                bucket['buy_notional'].append(buy_volume * vwap / 1e4)
            if sell_volume is not None and vwap > 0:
                bucket['sell_notional'].append(sell_volume * vwap / 1e4)
    stats = {label: {name: named_stat(values) for name, values in bucket.items()}
             for label, bucket in buckets.items()}
    return events, stats


def event_rows_csv(path, events):
    output_path = path + '.events.csv'
    with open(output_path, 'w', newline='') as output_file:
        writer = csv.writer(output_file)
        writer.writerow(['minute', 'cidx', 'event_z', 'close', 'amount', 'spread_bp',
                         'ask_depth', 'bid_depth', 'ask_notional', 'bid_notional',
                         'ask_l1_notional_share', 'bid_l1_notional_share',
                         'limit_up', 'limit_down', 'totask', 'totbid',
                         'ask_l1_depth_share', 'bid_l1_depth_share',
                         'buy_volume', 'sell_volume', 'buy_sell_imbalance',
                         'buy_notional', 'sell_notional'])
        for _, event_z, row in events:
            ask_notional = notional_proxy(row, 'ask')
            bid_notional = notional_proxy(row, 'bid')
            mid_price = (0.5 * (row['bid1'] + row['ask1']) / 1e4
                         if row['bid1'] > 0 and row['ask1'] > 0 else 0.0)
            writer.writerow([
                row['minute'], row['cidx'], event_z, row['close'], row['amount'],
                spread_bp(row),
                row['askvol1'] if row['askvol1'] > 0 else '',
                row['bidvol1'] if row['bidvol1'] > 0 else '',
                ask_notional if ask_notional is not None else '',
                bid_notional if bid_notional is not None else '',
                ((ask_notional / (mid_price * row['totask']))
                 if ask_notional is not None and mid_price > 0 and row['totask'] > 0 else ''),
                ((bid_notional / (mid_price * row['totbid']))
                 if bid_notional is not None and mid_price > 0 and row['totbid'] > 0 else ''),
                row['limitUp'] if row['limitUp'] > 0 else '',
                row['limitDown'] if row['limitDown'] > 0 else '',
                row['totask'] if row['totask'] > 0 else '',
                row['totbid'] if row['totbid'] > 0 else '',
                (row['askvol1'] / row['totask']) if row['askvol1'] > 0 and row['totask'] > 0 else '',
                (row['bidvol1'] / row['totbid']) if row['bidvol1'] > 0 and row['totbid'] > 0 else '',
                row['buyvol'] if row['buyvol'] > 0 else '',
                row['sellvol'] if row['sellvol'] > 0 else '',
                ((row['buyvol'] - row['sellvol']) / (row['buyvol'] + row['sellvol']))
                 if row['buyvol'] > 0 and row['sellvol'] > 0 else '',
                (row['buyvol'] * row['vwap'] / 1e4) if row['buyvol'] > 0 and row['vwap'] > 0 else '',
                (row['sellvol'] * row['vwap'] / 1e4) if row['sellvol'] > 0 and row['vwap'] > 0 else '',
            ])
    return output_path


def metric_line(label, metrics):
    if not metrics:
        return label
    values = []
    for name, active in (
        ('spread_bp', 'spread(p25/p50/p75)=%.2f/%.2f/%.2f bp'),
        ('ask_depth', 'askDepth(p50/p75)=%.0f/%.0f shares'),
        ('bid_depth', 'bidDepth(p50/p75)=%.0f/%.0f shares'),
        ('ask_notional', 'askNotional(p50/p75)=%.2f/%.2f yuan'),
        ('bid_notional', 'bidNotional(p50/p75)=%.2f/%.2f yuan'),
        ('bid_ask_depth_ratio', 'bid/askDepth(p50)=%.2f'),
        ('ask_l1_notional_share', 'askL1NotionalShare(p50/p75)=%.4f/%.4f'),
        ('bid_l1_notional_share', 'bidL1NotionalShare(p50/p75)=%.4f/%.4f'),
        ('buy_volume', 'buyVolume(p50/p75)=%.0f/%.0f shares'),
        ('sell_volume', 'sellVolume(p50/p75)=%.0f/%.0f shares'),
        ('buy_sell_imbalance', 'buySellImbalance(p50/p75)=%.3f/%.3f'),
        ('buy_notional', 'buyNotional(p50/p75)=%.2f/%.2f 万元'),
        ('sell_notional', 'sellNotional(p50/p75)=%.2f/%.2f 万元'),
    ):
        stat = metrics[name]
        if stat['median'] is None:
            values.append(name + '=na')
        elif name == 'spread_bp':
            values.append(active % (stat['p25'], stat['median'], stat['p75']))
        elif name == 'bid_ask_depth_ratio':
            values.append(active % stat['median'])
        else:
            values.append(active % (stat['median'], stat['p75']))
    return label + ': ' + ' | '.join(values)


def report(path, export_csv=False, horizons=None, exclude_limit=False):
    horizons = horizons or DEFAULT_HORIZONS
    if not path and path:
        pass
    rows = read_panelbook(path)
    events, stats = event_metrics(rows, horizons, exclude_limit=exclude_limit)
    print('panel', os.path.basename(path))
    print('rows', len(rows))
    print('events', len(events))
    for label in ['same'] + ['after_%d' % horizon for horizon in horizons]:
        print(metric_line(label, stats.get(label, {})))
    if export_csv:
        print('event_csv', event_rows_csv(path, events))
    return {'path': path, 'events': len(events), 'rows': len(rows), 'stats': stats}


def summarize_many(report_results, output_path, horizons):
    if output_path:
        fields = ['panel', 'period', 'events', 'metric', 'p25', 'p50', 'p75']
        with open(output_path, 'w', newline='') as output_file:
            writer = csv.DictWriter(output_file, fieldnames=fields)
            writer.writeheader()
            for result in report_results:
                for period in ['same'] + ['after_%d' % horizon for horizon in horizons]:
                    metrics = result['stats'].get(period)
                    if metrics:
                        for metric_name, named in metrics.items():
                            writer.writerow({'panel': result['path'], 'period': period,
                                             'events': result['events'], 'metric': metric_name,
                                             'p25': named['p25'], 'p50': named['median'],
                                             'p75': named['p75']})
            return output_path
    return output_path


def summarize_by_period(report_results, output_path):
    values = {}
    for result in report_results:
        for period, metrics in result['stats'].items():
            for metric_name, named in metrics.items():
                if named['median'] is not None:
                    values.setdefault((period, metric_name), []).append(named['median'])
    fields = ['period', 'metric', 'day_count', 'cross_day_median', 'cross_day_mean',
              'day_p25', 'day_p75']
    with open(output_path, 'w', newline='') as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fields)
        writer.writeheader()
        for (period, metric_name), day_values in sorted(values.items()):
            ordered = sorted(day_values)
            quantiles = statistics.quantiles(ordered, n=4, method='inclusive') if len(ordered) > 1 else [ordered[0], ordered[0], ordered[0]]
            writer.writerow({
                'period': period,
                'metric': metric_name,
                'day_count': len(day_values),
                'cross_day_median': statistics.median(ordered),
                'cross_day_mean': statistics.mean(ordered),
                'day_p25': quantiles[0],
                'day_p75': quantiles[2],
            })
    return output_path


def summarize_period_pivot(report_results, output_path, horizons):
    values = {}
    for result in report_results:
        for period, metrics in result['stats'].items():
            for metric_name, named in metrics.items():
                if named['median'] is not None:
                    values.setdefault((metric_name, period), []).append(named['median'])
    period_labels = ['same'] + ['after_%d' % horizon for horizon in horizons]
    fieldnames = ['metric', 'day_count'] + period_labels
    with open(output_path, 'w', newline='') as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fieldnames)
        writer.writeheader()
        for metric_name in sorted({key[0] for key in values}):
            row = {'metric': metric_name}
            for period in period_labels:
                day_values = values.get((metric_name, period), [])
                if day_values:
                    row['day_count'] = len(day_values)
                    row[period] = statistics.median(sorted(day_values))
            writer.writerow(row)
    return output_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('panels', nargs='*')
    parser.add_argument('--csv', action='store_true')
    parser.add_argument('--horizons', default='1,3,5,10,15,30,45,60')
    parser.add_argument('--summary', default='')
    parser.add_argument('--period-summary', default='')
    parser.add_argument('--period-pivot', default='')
    parser.add_argument('--exclude-limit', action='store_true')
    arguments = parser.parse_args()
    horizons = tuple(int(value) for value in arguments.horizons.split(',') if value)
    paths = arguments.panels or sorted(glob.glob('pipeline/panel_books/*.panelbook'))
    results = []
    for path in paths:
        if os.path.isfile(path):
            results.append(report(path, arguments.csv, horizons, arguments.exclude_limit))
        elif '*' in path or '?' in path:
            for matched_path in sorted(glob.glob(path)):
                results.append(report(matched_path, arguments.csv, horizons, arguments.exclude_limit))
        else:
            raise FileNotFoundError(path)
    summary_path = summarize_many(results, arguments.summary, horizons)
    if summary_path:
        print('summary_csv', summary_path)
    period_summary_path = summarize_by_period(results, arguments.period_summary) if arguments.period_summary else None
    if period_summary_path:
        print('period_summary_csv', period_summary_path)
    period_pivot_path = summarize_period_pivot(
        results, arguments.period_pivot, horizons) if arguments.period_pivot else None
    if period_pivot_path:
        print('period_pivot_csv', period_pivot_path)


if __name__ == '__main__':
    main()
