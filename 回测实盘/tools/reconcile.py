#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""逐字段对账 fast_reader 输出与 pipeline 分钟面板。

用法:
    python3 -I tools/reconcile.py [fast.panel] [ref.panel]

输出:
    - 行数、结构是否一致
    - 完全一致（整数相等 + 浮点位模式相等，NaN 视为相等）的行占比
    - 各字段最大绝对误差 / 超过阈值的行数
    - 前 5 个不一致样本（cidx + minute + 两个值）
"""
import struct
import sys
import math

FAST_DEFAULT = "/home/intern2/rachel/回测实盘/out/20260105.fast.panel"
REF_DEFAULT = "/home/intern2/rachel/pipeline/panels/20260105.panel"
ROW = 48
FMT = "<5i7f"
FIELDS = ["nTime", "minute", "cidx", "nMd", "nTs",
          "open", "high", "low", "close", "vwap", "volume", "amount"]
NF = len(FIELDS)


def load(path):
    with open(path, "rb") as f:
        d = f.read()
    if len(d) % ROW:
        raise SystemExit("%s: size %d not multiple of %d" % (path, len(d), ROW))
    return d


def fbits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


def main():
    fast_path = sys.argv[1] if len(sys.argv) > 1 else FAST_DEFAULT
    ref_path = sys.argv[2] if len(sys.argv) > 2 else REF_DEFAULT
    a = load(fast_path)
    b = load(ref_path)
    n = len(a) // ROW
    m = len(b) // ROW
    print("fast=%s rows=%d" % (fast_path, n))
    print("ref =%s rows=%d" % (ref_path, m))
    if n != m:
        print("!! 行数不一致: %d vs %d" % (n, m))
    rows = min(n, m)

    identical = 0
    float_exact = 0  # 浮点位模式完全一致的行数
    int_exact = 0
    maxerr = [0.0] * NF
    nbad = [0] * NF          # 每个字段不一致的行数
    samples = []
    sum_md_a = sum_md_b = sum_ts_a = sum_ts_b = 0
    sum_vol_a = sum_vol_b = 0.0
    sum_amt_a = sum_amt_b = 0.0
    nan_rows = 0

    for k in range(rows):
        ra = struct.unpack_from(FMT, a, k * ROW)
        rb = struct.unpack_from(FMT, b, k * ROW)
        ints_ok = True
        floats_ok = True
        row_sample = None
        for j in range(NF):
            x, y = ra[j], rb[j]
            if j < 5:
                if x != y:
                    ints_ok = False
                    nbad[j] += 1
                    e = abs(x - y)
                    if e > maxerr[j]:
                        maxerr[j] = e
                    if row_sample is None:
                        row_sample = (j, x, y)
            else:
                xa_nan = math.isnan(x)
                xb_nan = math.isnan(y)
                if xa_nan or xb_nan:
                    if not (xa_nan and xb_nan):
                        floats_ok = False
                        nbad[j] += 1
                        if row_sample is None:
                            row_sample = (j, x, y)
                    continue
                e = abs(x - y)
                if e > maxerr[j]:
                    maxerr[j] = e
                if fbits(x) != fbits(y):
                    floats_ok = False
                    nbad[j] += 1
                    if row_sample is None:
                        row_sample = (j, x, y)
        ident = ints_ok and floats_ok
        if ident:
            identical += 1
            float_exact += 1
        if ints_ok:
            int_exact += 1
        if math.isnan(ra[5]) and math.isnan(rb[5]):
            nan_rows += 1
        if row_sample is not None and len(samples) < 5:
            samples.append((ra[2], ra[1], ra[0], FIELDS[row_sample[0]],
                            row_sample[1], row_sample[2]))
        sum_md_a += ra[3]; sum_md_b += rb[3]
        sum_ts_a += ra[4]; sum_ts_b += rb[4]
        if not math.isnan(ra[10]): sum_vol_a += ra[10]
        if not math.isnan(rb[10]): sum_vol_b += rb[10]
        if not math.isnan(ra[11]): sum_amt_a += ra[11]
        if not math.isnan(rb[11]): sum_amt_b += rb[11]

    print("compared_rows=%d" % rows)
    print("identical_rows=%d ratio=%.6f%%" % (identical, 100.0 * identical / rows if rows else 0))
    print("int_fields_identical_rows=%d ratio=%.6f%%" % (int_exact, 100.0 * int_exact / rows if rows else 0))
    print("nan_price_rows=%d" % nan_rows)
    print("sum_nMd fast=%d ref=%d diff=%d" % (sum_md_a, sum_md_b, sum_md_a - sum_md_b))
    print("sum_nTs fast=%d ref=%d diff=%d" % (sum_ts_a, sum_ts_b, sum_ts_a - sum_ts_b))
    print("sum_volume fast=%.3f ref=%.3f diff=%.3f" % (sum_vol_a, sum_vol_b, sum_vol_a - sum_vol_b))
    print("sum_amount fast=%.3f ref=%.3f diff=%.3f" % (sum_amt_a, sum_amt_b, sum_amt_a - sum_amt_b))
    print("")
    print("%-8s %14s %12s" % ("field", "max_abs_err", "bad_rows"))
    for j in range(NF):
        print("%-8s %14.6g %12d" % (FIELDS[j], maxerr[j], nbad[j]))
    print("")
    print("first_mismatches (cidx, minute, nTime, field, fast, ref):")
    for s in samples:
        print("  cidx=%d minute=%d nTime=%d %s fast=%r ref=%r" % s)
    if not samples:
        print("  (none)")


if __name__ == "__main__":
    main()
