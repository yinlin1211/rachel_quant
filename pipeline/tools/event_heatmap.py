#!/usr/bin/env python3
"""Print a 5-minute event-count heatmap from one events.csv file."""
import argparse
import csv
from collections import defaultdict
from datetime import datetime, timedelta


def minute_floor(minute, step_minutes):
    current = datetime.strptime('%06d' % minute, '%H%M%S')
    minute_count = current.hour * 60 + current.minute
    rounded = (minute_count // step_minutes) * step_minutes
    return datetime.combine(current.date(), datetime.min.time()) + timedelta(minutes=rounded)


def print_heatmap(csv_path, step_minutes=5, symbol='#', max_bars=10):
    counts = defaultdict(int)
    with open(csv_path, newline='') as events_file:
        reader = csv.DictReader(events_file)
        for row in reader:
            counts[minute_floor(int(row['minute']), step_minutes)] += 1
    maximum = max(counts.values(), default=0)
    if maximum == 0:
        print('events 0')
        return
    scale = float(max_bars) / maximum
    for bucket, count in sorted(counts.items()):
        width = max(1, round(count * scale)) if count else 0
        print('%s %2d %s' % (bucket.strftime('%H:%M'), count, symbol * width))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('events_csv')
    parser.add_argument('--step', type=int, default=5)
    parser.add_argument('--max-bars', type=int, default=10)
    arguments = parser.parse_args()
    print_heatmap(arguments.events_csv, arguments.step, '#', arguments.max_bars)


if __name__ == '__main__':
    main()
