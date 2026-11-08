#!/usr/bin/env python3
"""Aggregate event-hour money-to-money pressure across panelbooks."""
import argparse
import csv
import glob
import sys
import statistics
from collections import defaultdict

try:
    from event_spread import event_rows, notional_proxy, read_panelbook, stock_rows
except ModuleNotFoundError:
    from pipeline.tools.event_spread import event_rows, notional_proxy, read_panelbook, stock_rows


def build_hour_cells(paths):
    """Return per-hour and per-day-hour money-to-money pressure cells."""
    hour_cells = defaultdict(lambda: defaultdict(list))
    day_hour_cells = defaultdict(lambda: defaultdict(list))
    for path in paths:
        day = path.rsplit('/', 1)[-1].split('.')[0]
        grouped_rows = stock_rows(read_panelbook(path))
        for _, _, row in event_rows(grouped_rows):
            ask_notional = notional_proxy(row, 'ask')
            bid_notional = notional_proxy(row, 'bid')
            if ask_notional is None or bid_notional is None or ask_notional <= 0 or bid_notional <= 0:
                continue
            if (row['limitUp'] > 0 and row['close'] >= row['limitUp']) or (
                    row['limitDown'] > 0 and row['close'] <= row['limitDown']):
                continue
            hour = row['minute'] // 10000
            if row['buyvol'] > 0 and row['vwap'] > 0:
                buy_notional = row['buyvol'] * row['vwap'] / 1e4
                ratio = buy_notional * 10000 / ask_notional
                hour_cells[hour]['buy'].append(ratio)
                day_hour_cells[(day, hour)]['buy'].append(ratio)
            if row['sellvol'] > 0 and row['vwap'] > 0:
                sell_notional = row['sellvol'] * row['vwap'] / 1e4
                ratio = sell_notional * 10000 / bid_notional
                hour_cells[hour]['sell'].append(ratio)
                day_hour_cells[(day, hour)]['sell'].append(ratio)
            if (row['buyvol'] > 0 and row['sellvol'] > 0 and row['vwap'] > 0):
                hour_cells[hour]['both'].append(1)
                day_hour_cells[(day, hour)]['both'].append(1)
    return hour_cells, day_hour_cells


def write_pressure_csv(output_path, hour_cells, day_hour_cells):
    fields = ['hour', 'event_count', 'buy_median', 'sell_median', 'buy_over_1',
              'sell_over_1', 'daily_buy_median', 'daily_sell_median']
    with open(output_path, 'w', newline='') as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fields)
        writer.writeheader()
        for hour, cells in sorted(hour_cells.items()):
            buy = cells.get('buy')
            sell = cells.get('sell')
            both_count = len(cells.get('both', []))
            if not buy or not sell:
                continue
            days = sorted({day for day, cell_hour in day_hour_cells
                           if cell_hour == hour and day_hour_cells[(day, cell_hour)].get('both')})
            writer.writerow({
                'hour': f'{hour:02d}',
                'event_count': both_count,
                'buy_median': statistics.median(buy),
                'sell_median': statistics.median(sell),
                'buy_over_1': sum(value > 1 for value in buy) / len(buy),
                'sell_over_1': sum(value > 1 for value in sell) / len(sell),
                'daily_buy_median': statistics.median(
                    statistics.median(day_hour_cells[(day, hour)]['buy']) for day in days),
                'daily_sell_median': statistics.median(
                    statistics.median(day_hour_cells[(day, hour)]['sell']) for day in days),
            })
    return output_path


def write_daily_pressure_csv(output_path, day_hour_cells):
    fields = ['day', 'hour', 'event_count', 'buy_median', 'sell_median',
              'buy_over_1', 'sell_over_1']
    with open(output_path, 'w', newline='') as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fields)
        writer.writeheader()
        for (day, hour), cells in sorted(day_hour_cells.items()):
            buy = cells.get('buy')
            sell = cells.get('sell')
            both_count = len(cells.get('both', []))
            if not buy or not sell or not both_count:
                continue
            writer.writerow({
                'day': day,
                'hour': f'{hour:02d}',
                'event_count': both_count,
                'buy_median': statistics.median(buy),
                'sell_median': statistics.median(sell),
                'buy_over_1': sum(value > 1 for value in buy) / len(buy),
                'sell_over_1': sum(value > 1 for value in sell) / len(sell),
            })
    return output_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('panels', nargs='*')
    parser.add_argument('--output', default='pipeline/panel_books/event_hourly_pressure.csv')
    parser.add_argument(
        '--daily-output',
        default='pipeline/panel_books/event_hourly_pressure_daily.csv',
    )
    args = parser.parse_args()
    paths = args.panels or sorted(glob.glob('pipeline/panel_books/*.panelbook'))
    hour_cells, day_hour_cells = build_hour_cells(paths)
    written = [
        write_pressure_csv(args.output, hour_cells, day_hour_cells),
        write_daily_pressure_csv(args.daily_output, day_hour_cells),
    ]
    print('output', *written)


if __name__ == '__main__':
    main()
