#!/usr/bin/env python3
"""
E3：订单流毒性与逆向选择 —— 基于盘口面板（panelbook）

数据：pipeline/panel_books/<YYYYMMDD>.panelbook
      92 字节定长行（packed）：5×int32 + 18×float
      字段：nTime, minute, cidx, nMd, nTs | open, high, low, close, vwap, volume, amount,
            bid1, ask1, bidvol1, askvol1, totbid, totask, buyvol, sellvol, ofi,
            limitUp, limitDown

因子（成交口径的主动买卖，比 OD 的委托口径更接近"真实毒性"）：
  exec_imb       (buyvol - sellvol) / (buyvol + sellvol)      主动买卖失衡
  ofi_panelbook  panelbook 自带的 OFI 字段
  vpin_proxy     |buyvol - sellvol| / (buyvol + sellvol)       近似 VPIN（量失衡强度）
  depth_imb      (bidvol1 - askvol1) / (bidvol1 + askvol1)     盘口深度失衡

检验：
  1) 对 1/3/5 分钟前向收益的横截面 RankIC —— 是否可预测
  2) "毒性"定义：按 exec_imb 符号分组的**成交后价格漂移**
     （主动买入后价格是否继续涨 = 知情交易；不涨反跌 = 逆向选择）

输出：experiments/e3_toxicity/e3_results.csv + e3_toxicity.csv
"""
import numpy as np
import csv
import os
import glob
import sys

BASE = "/home/intern2/rachel"
PB_DIR = f"{BASE}/pipeline/panel_books"
OUT = f"{BASE}/experiments/e3_toxicity"

ROW = 92
DT = np.dtype([
    ("nTime", "<i4"), ("minute", "<i4"), ("cidx", "<i4"), ("nMd", "<i4"), ("nTs", "<i4"),
    ("open", "<f4"), ("high", "<f4"), ("low", "<f4"), ("close", "<f4"), ("vwap", "<f4"),
    ("volume", "<f4"), ("amount", "<f4"),
    ("bid1", "<f4"), ("ask1", "<f4"), ("bidvol1", "<f4"), ("askvol1", "<f4"),
    ("totbid", "<f4"), ("totask", "<f4"), ("buyvol", "<f4"), ("sellvol", "<f4"),
    ("ofi", "<f4"), ("limitUp", "<f4"), ("limitDown", "<f4"),
])
assert DT.itemsize == ROW, DT.itemsize

T, N = 240, 5213


def slot_of_hhmmss(v):
    """panelbook 的 minute 字段是 HHMMSS（如 93000 = 9:30:00），不是距零点分钟数。"""
    hhmm = int(v) // 100
    h, m = hhmm // 100, hhmm % 100
    mod = h * 60 + m
    if 570 <= mod < 690:
        return mod - 570
    if 780 <= mod < 900:
        return 120 + (mod - 780)
    return -1


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
    os.makedirs(OUT, exist_ok=True)
    files = sorted(glob.glob(f"{PB_DIR}/*.panelbook"))
    if not files:
        sys.exit("没有 panelbook")
    print(f"找到 {len(files)} 天 panelbook")

    factors = ["exec_imb", "ofi_panelbook", "vpin_proxy", "depth_imb"]
    horizons = [1, 3, 5]
    per_day = {f: {h: [] for h in horizons} for f in factors}
    # 毒性：按 exec_imb 十分位，统计未来 3 分钟收益（bps）
    decile_ret = {d: [] for d in range(10)}

    for path in files:
        date = os.path.basename(path).split(".")[0]
        a = np.fromfile(path, dtype=DT)
        if a.size == 0:
            continue
        close = np.full((T, N), np.nan)
        buyv = np.zeros((T, N)); sellv = np.zeros((T, N))
        ofi = np.zeros((T, N)); bv1 = np.zeros((T, N)); av1 = np.zeros((T, N))
        vol = np.zeros((T, N))
        for r in a:
            s = slot_of_hhmmss(int(r["minute"]))
            c = int(r["cidx"])
            if s < 0 or not (0 <= c < N):
                continue
            cv = float(r["close"])
            close[s, c] = cv if cv > 0 else np.nan
            buyv[s, c] = float(r["buyvol"]); sellv[s, c] = float(r["sellvol"])
            ofi[s, c] = float(r["ofi"])
            bv1[s, c] = float(r["bidvol1"]); av1[s, c] = float(r["askvol1"])
            vol[s, c] = float(r["volume"])

        fwd = {}
        for h in horizons:
            r = np.full((T, N), np.nan)
            r[: T - h] = close[h:] / close[: T - h] - 1.0
            r[119, :] = np.nan
            fwd[h] = r

        day_ic = {f: {h: [] for h in horizons} for f in factors}
        for s in range(T):
            act = (buyv[s] + sellv[s]) > 0
            if act.sum() < 200:
                continue
            tot = buyv[s] + sellv[s]
            f_exec = np.where(act, (buyv[s] - sellv[s]) / np.maximum(tot, 1.0), np.nan)
            f_vpin = np.where(act, np.abs(buyv[s] - sellv[s]) / np.maximum(tot, 1.0), np.nan)
            f_ofi = np.where(act, ofi[s], np.nan)
            dt_ = bv1[s] + av1[s]
            f_depth = np.where(dt_ > 0, (bv1[s] - av1[s]) / np.maximum(dt_, 1.0), np.nan)
            fs = {"exec_imb": f_exec, "ofi_panelbook": f_ofi,
                  "vpin_proxy": f_vpin, "depth_imb": f_depth}
            for fname, fv in fs.items():
                for h in horizons:
                    ic = rank_ic(fv, fwd[h][s])
                    if np.isfinite(ic):
                        day_ic[fname][h].append(ic)
            # 毒性：exec_imb 十分位 × 未来 3 分钟收益
            r3 = fwd[3][s]
            m = np.isfinite(f_exec) & np.isfinite(r3)
            if m.sum() >= 200:
                x, y = f_exec[m], r3[m]
                q = np.argsort(np.argsort(x)) * 10 // len(x)
                for d in range(10):
                    sel = y[q == d]
                    if sel.size:
                        decile_ret[d].append(float(sel.mean()) * 1e4)

        for fname in factors:
            for h in horizons:
                if day_ic[fname][h]:
                    per_day[fname][h].append(np.mean(day_ic[fname][h]))
        print(f"  {date}: 完成")

    print("\n" + "=" * 76)
    print(f"{'因子':<16}{'IC1':>9}{'IR1':>8}{'IC3':>9}{'IR3':>8}{'IC5':>9}{'IR5':>8}")
    print("=" * 76)
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
        print(f"{fname:<16}{vals[0]:>9.4f}{vals[1]:>8.2f}{vals[2]:>9.4f}{vals[3]:>8.2f}"
              f"{vals[4]:>9.4f}{vals[5]:>8.2f}")
        rows.append([fname] + [f"{v:.6f}" if np.isfinite(v) else "" for v in vals])
    print("=" * 76)

    with open(f"{OUT}/e3_results.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["factor", "IC1", "IR1", "IC3", "IR3", "IC5", "IR5"])
        w.writerows(rows)

    # 毒性表
    print("\n主动买卖失衡十分位 → 未来 3 分钟收益（bps）")
    print(f"{'十分位':>8}{'样本':>12}{'未来3min收益':>16}")
    with open(f"{OUT}/e3_toxicity.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["decile", "n", "fwd3_bps"])
        for d in range(10):
            arr = decile_ret[d]
            if arr:
                print(f"{d:>8}{len(arr):>12}{np.mean(arr):>15.2f}")
                w.writerow([d, len(arr), f"{np.mean(arr):.4f}"])
    print(f"\n已写入 {OUT}/")


if __name__ == "__main__":
    main()
