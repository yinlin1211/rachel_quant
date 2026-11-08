#!/usr/bin/env python3
"""
E5：拆单与冰山单识别 —— IC 检验

数据：experiments/e5_split/od_split_<date>.csv
      由 pipeline/bin/od_split 从 TickPool 逐笔委托直接扫描得到
      字段：date,cidx,slot,n_orders,n_seq_same,n_idx_consec

因子（代理「算法拆单」强度）：
  split_ratio     = n_seq_same / n_orders     同价同向 5 秒内的连续报单占比
  idx_consec_ratio= n_idx_consec / n_orders    委托编号连续的占比
  split_intensity = n_seq_same / (n_orders+1) × log(1+n_orders)   强度×活跃度

检验：横截面 RankIC（每日每分钟），对 1/3/5 分钟前向收益。
输出：experiments/e5_split/e5_results.csv
"""
import numpy as np
import csv
import os
import glob
import sys

BASE = "/home/intern2/rachel"
PANEL_DIR = f"{BASE}/pipeline/panels"
SPLIT_DIR = f"{BASE}/experiments/e5_split"
OUT = f"{SPLIT_DIR}/e5_results.csv"

ROW = 48
FMT = np.dtype([
    ("date", "<i4"), ("time", "<i4"), ("cidx", "<i4"), ("u1", "<i4"), ("u2", "<i4"),
    ("open", "<f4"), ("high", "<f4"), ("low", "<f4"), ("close", "<f4"),
    ("vwap", "<f4"), ("volume", "<f4"), ("amount", "<f4"),
])
T, N = 240, 5213


def slot_of_hhmmss(hhmmss):
    hhmm = int(hhmmss) // 100
    h, m = hhmm // 100, hhmm % 100
    mod = h * 60 + m
    if 570 <= mod < 690:
        return mod - 570
    if 780 <= mod < 900:
        return 120 + (mod - 780)
    return -1


def load_panel(date):
    path = f"{PANEL_DIR}/{date}.panel"
    if not os.path.exists(path):
        return None
    a = np.fromfile(path, dtype=FMT)
    close = np.full((T, N), np.nan)
    for r in a:
        s = slot_of_hhmmss(int(r["time"]))
        c = int(r["cidx"])
        if s >= 0 and 0 <= c < N:
            v = float(r["close"])
            close[s, c] = v if v > 0 else np.nan
    return close


def rank_ic(f, r):
    m = np.isfinite(f) & np.isfinite(r)
    if m.sum() < 100:
        return np.nan
    x, y = f[m], r[m]
    rx = np.argsort(np.argsort(x)).astype(np.float64)
    ry = np.argsort(np.argsort(y)).astype(np.float64)
    rx -= rx.mean(); ry -= ry.mean()
    d = np.sqrt((rx * rx).sum() * (ry * ry).sum())
    return float((rx * ry).sum() / d) if d > 0 else np.nan


def main():
    files = sorted(glob.glob(f"{SPLIT_DIR}/od_split_*.csv"))
    if not files:
        sys.exit("没有 od_split 输出")
    factors = ["split_ratio", "idx_consec_ratio", "split_intensity"]
    horizons = [1, 3, 5]
    per_day = {f: {h: [] for h in horizons} for f in factors}
    # 拆单强度 × 未来收益（分档）
    q_ret = {q: [] for q in range(5)}
    total_cover = []

    for path in files:
        date = os.path.basename(path).split("_")[-1].split(".")[0]
        close = load_panel(date)
        if close is None:
            print(f"  {date}: 无面板，跳过")
            continue

        n_ord = np.zeros((T, N)); n_seq = np.zeros((T, N)); n_cons = np.zeros((T, N))
        with open(path) as f:
            r = csv.reader(f)
            next(r, None)
            for row in r:
                if len(row) < 6:
                    continue
                c = int(row[1]); s = int(row[2])
                if not (0 <= c < N and 0 <= s < T):
                    continue
                n_ord[s, c] = float(row[3])
                n_seq[s, c] = float(row[4])
                n_cons[s, c] = float(row[5])

        fwd = {}
        for h in horizons:
            rr = np.full((T, N), np.nan)
            rr[: T - h] = close[h:] / close[: T - h] - 1.0
            rr[119, :] = np.nan
            fwd[h] = rr

        day_ic = {f: {h: [] for h in horizons} for f in factors}
        for s in range(T):
            act = n_ord[s] >= 20          # 至少 20 笔委托才有意义
            if act.sum() < 100:
                continue
            with np.errstate(invalid="ignore", divide="ignore"):
                f_split = np.where(act, n_seq[s] / n_ord[s], np.nan)
                f_cons = np.where(act, n_cons[s] / n_ord[s], np.nan)
                f_int = np.where(act, n_seq[s] / (n_ord[s] + 1.0) * np.log1p(n_ord[s]), np.nan)
            fs = {"split_ratio": f_split, "idx_consec_ratio": f_cons, "split_intensity": f_int}
            for fname, fv in fs.items():
                for h in horizons:
                    ic = rank_ic(fv, fwd[h][s])
                    if np.isfinite(ic):
                        day_ic[fname][h].append(ic)
            total_cover.append(int(act.sum()))

            # 分档：拆单强度五分位 × 未来 3 分钟收益
            r3 = fwd[3][s]
            m = np.isfinite(f_int) & np.isfinite(r3)
            if m.sum() >= 200:
                x, y = f_int[m], r3[m]
                q = np.argsort(np.argsort(x)) * 5 // len(x)
                for k in range(5):
                    sel = y[q == k]
                    if sel.size:
                        q_ret[k].append(float(sel.mean()) * 1e4)

        for fname in factors:
            for h in horizons:
                if day_ic[fname][h]:
                    per_day[fname][h].append(np.mean(day_ic[fname][h]))
        print(f"  {date}: 完成")

    print("\n" + "=" * 74)
    print(f"{'因子':<20}{'IC1':>9}{'IR1':>8}{'IC3':>9}{'IR3':>8}{'IC5':>9}{'IR5':>8}")
    print("=" * 74)
    rows = []
    for fname in factors:
        vals = []
        for h in horizons:
            arr = np.array(per_day[fname][h], dtype=np.float64)
            arr = arr[np.isfinite(arr)]
            if arr.size == 0:
                vals += [np.nan, np.nan]
            else:
                ic = arr.mean()
                ir = ic / arr.std(ddof=1) * np.sqrt(arr.size) if arr.std(ddof=1) > 0 else np.nan
                vals += [ic, ir]
        print(f"{fname:<20}{vals[0]:>9.4f}{vals[1]:>8.2f}{vals[2]:>9.4f}{vals[3]:>8.2f}"
              f"{vals[4]:>9.4f}{vals[5]:>8.2f}")
        rows.append([fname] + [f"{v:.6f}" if np.isfinite(v) else "" for v in vals])
    print("=" * 74)
    print(f"有效截面平均覆盖：{np.mean(total_cover):.0f} 只股票/分钟")

    with open(OUT, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["factor", "IC1", "IR1", "IC3", "IR3", "IC5", "IR5"])
        w.writerows(rows)

    print("\n拆单强度五分位 → 未来 3 分钟收益（bps）")
    for k in range(5):
        if q_ret[k]:
            print(f"  Q{k+1}: {np.mean(q_ret[k]):>7.2f}  (n={len(q_ret[k])})")
    with open(f"{SPLIT_DIR}/e5_quintile.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["quintile", "n", "fwd3_bps"])
        for k in range(5):
            if q_ret[k]:
                w.writerow([k + 1, len(q_ret[k]), f"{np.mean(q_ret[k]):.4f}"])
    print(f"\n已写入 {OUT}")


if __name__ == "__main__":
    main()
