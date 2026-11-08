#!/usr/bin/env python3
"""
E2：订单生命周期与撤单行为因子 —— IC 检验

数据：
  撤单特征  experiments/e2_cancel/cancel_agg.csv   （ClickHouse od 表按 股票×分钟 聚合）
  分钟行情  pipeline/panels/<YYYYMMDD>.panel        （48B 定长，slot 外层 × cidx 内层）

因子（全部为「该分钟内的订单流行为」，与已有价量因子正交）：
  cxl_rate      撤单笔数 / 新增笔数
  cxl_vol_rate  撤单量 / 新增量
  cxl_imb       (买撤 - 卖撤) / (买撤 + 卖撤)      卖压撤销 → 看涨
  add_imb       (买增 - 卖增) / (买增 + 卖增)      主动买盘 → 看涨
  cxl_add_imb   撤单失衡 − 新增失衡（撤单相对新增的异常）

检验：横截面 RankIC（每日每分钟），对 1/3/5 分钟前向收益。
输出：experiments/e2_cancel/e2_results.csv + 控制台摘要
"""
import numpy as np
import csv
import sys
import os

BASE = "/home/intern2/rachel"
PANEL_DIR = f"{BASE}/pipeline/panels"
AGG = f"{BASE}/experiments/e2_cancel/cancel_agg.csv"
OUT = f"{BASE}/experiments/e2_cancel/e2_results.csv"

ROW = 48
FMT = np.dtype([
    ("date", "<i4"), ("time", "<i4"), ("cidx", "<i4"), ("u1", "<i4"), ("u2", "<i4"),
    ("open", "<f4"), ("high", "<f4"), ("low", "<f4"), ("close", "<f4"),
    ("vwap", "<f4"), ("volume", "<f4"), ("amount", "<f4"),
])
assert FMT.itemsize == ROW, FMT.itemsize

T, N = 240, 5213


def slot_of(hhmmss: int):
    """面板 240 槽：9:30-11:29 -> 0..119，13:00-14:59 -> 120..239。

    注意：hhmmss 是 HHMMSSmmm 的十进制表示，不能直接用 `hhmm - 930` 算槽位
    （跨小时后会错位，例如 10:00 会算成 70 而正确值是 30）。
    必须先折算成「距零点的分钟数」。
    """
    hhmm = hhmmss // 100
    h, m = hhmm // 100, hhmm % 100
    return slot_of_minute(h * 60 + m)


def slot_of_minute(mod: int):
    """od 表的 mod = 距零点的分钟数。"""
    if 570 <= mod < 690:
        return mod - 570
    if 780 <= mod < 900:
        return 120 + (mod - 780)
    return -1


def load_panel(date: str):
    path = f"{PANEL_DIR}/{date}.panel"
    if not os.path.exists(path):
        return None
    a = np.fromfile(path, dtype=FMT)
    close = np.full((T, N), np.nan, dtype=np.float64)
    for r in a:
        s = slot_of(int(r["time"]))
        c = int(r["cidx"])
        if s >= 0 and 0 <= c < N:
            v = float(r["close"])
            close[s, c] = v if v > 0 else np.nan
    return close


def load_cancel_agg():
    """-> dict[(date, slot)] = (cxl_cnt, add_cnt, cxl_vol, add_vol, cxl_b, cxl_s, add_b, add_s) 各为 (N,) 数组"""
    out = {}
    if not os.path.exists(AGG):
        sys.exit(f"缺少 {AGG}，先跑 ClickHouse 聚合")
    with open(AGG) as f:
        for row in csv.reader(f):
            if len(row) < 11:
                continue
            try:
                date = row[0].strip()
                cidx = int(row[1])
                mod = int(row[2])
            except ValueError:
                continue
            s = slot_of_minute(mod)
            if s < 0 or not (0 <= cidx < N):
                continue
            key = (date, s)
            if key not in out:
                out[key] = np.zeros((8, N), dtype=np.float64)
            out[key][:, cidx] = [float(x) for x in row[3:11]]
    return out


def rank_ic(f, r):
    """横截面 RankIC（Spearman）：只在两者都有效的股票上算。"""
    m = np.isfinite(f) & np.isfinite(r)
    if m.sum() < 100:
        return np.nan
    x, y = f[m], r[m]
    rx = np.argsort(np.argsort(x))
    ry = np.argsort(np.argsort(y))
    rx = rx - rx.mean()
    ry = ry - ry.mean()
    d = np.sqrt((rx * rx).sum() * (ry * ry).sum())
    return float((rx * ry).sum() / d) if d > 0 else np.nan


def main():
    agg = load_cancel_agg()
    if not agg:
        sys.exit("聚合结果为空")
    dates = sorted({d for d, _ in agg})
    print(f"撤单聚合：{len(dates)} 天，{len(agg)} 个（日,分钟）截面")

    factors = ["cxl_rate", "cxl_vol_rate", "cxl_imb", "add_imb", "cxl_add_imb"]
    horizons = [1, 3, 5]
    per_day = {f: {h: [] for h in horizons} for f in factors}
    cover = {f: [] for f in factors}

    for date in dates:
        close = load_panel(date)
        if close is None:
            print(f"  {date}: 无面板，跳过")
            continue
        # 前向收益
        fwd = {}
        for h in horizons:
            r = np.full((T, N), np.nan)
            r[: T - h] = close[h:] / close[: T - h] - 1.0
            r[119, :] = np.nan   # 跨午休
            fwd[h] = r

        day_ic = {f: {h: [] for h in horizons} for f in factors}
        for s in range(T):
            key = (date, s)
            if key not in agg:
                continue
            cult = agg[key]
            cxl_cnt, add_cnt, cxl_vol, add_vol, cxl_b, cxl_s, add_b, add_s = cult

            f_cxl_rate = cxl_cnt / (add_cnt + 1.0)
            f_cxl_vol_rate = cxl_vol / (add_vol + 1.0)
            f_cxl_imb = (cxl_b - cxl_s) / (cxl_b + cxl_s + 1.0)
            f_add_imb = (add_b - add_s) / (add_b + add_s + 1.0)
            fs = {
                "cxl_rate": f_cxl_rate,
                "cxl_vol_rate": f_cxl_vol_rate,
                "cxl_imb": f_cxl_imb,
                "add_imb": f_add_imb,
                "cxl_add_imb": f_cxl_imb - f_add_imb,
            }
            # 只保留有实际订单活动的股票
            active = add_cnt >= 10
            for fname, fv in fs.items():
                v = np.where(active, fv, np.nan)
                for h in horizons:
                    ic = rank_ic(v, fwd[h][s])
                    if np.isfinite(ic):
                        day_ic[fname][h].append(ic)
                if s % 40 == 0:
                    cover[fname].append(int(np.isfinite(v).sum()))

        for fname in factors:
            for h in horizons:
                if day_ic[fname][h]:
                    per_day[fname][h].append(np.mean(day_ic[fname][h]))
        print(f"  {date}: 完成（{len(day_ic['cxl_rate'][1])} 个截面有效）")

    # 汇总
    print("\n" + "=" * 78)
    print(f"{'因子':<14}{'IC1':>9}{'IR1':>8}{'IC3':>9}{'IR3':>8}{'IC5':>9}{'IR5':>8}{'覆盖':>8}")
    print("=" * 78)
    rows = []
    for fname in factors:
        vals = []
        for h in horizons:
            arr = np.array(per_day[fname][h], dtype=np.float64)
            arr = arr[np.isfinite(arr)]
            if len(arr) == 0:
                vals += [np.nan, np.nan]
            else:
                ic = arr.mean()
                ir = ic / arr.std(ddof=1) * np.sqrt(len(arr)) if arr.std(ddof=1) > 0 else np.nan
                vals += [ic, ir]
        cv = np.mean(cover[fname]) if cover[fname] else np.nan
        print(f"{fname:<14}{vals[0]:>9.4f}{vals[1]:>8.2f}{vals[2]:>9.4f}{vals[3]:>8.2f}"
              f"{vals[4]:>9.4f}{vals[5]:>8.2f}{cv:>8.0f}")
        rows.append([fname] + [f"{v:.6f}" if np.isfinite(v) else "" for v in vals] + [f"{cv:.0f}"])
    print("=" * 78)
    print("IC = 每日横截面 RankIC 的日间均值；IR = IC/ICstd × sqrt(天数)")

    with open(OUT, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["factor", "IC1", "IR1", "IC3", "IR3", "IC5", "IR5", "coverage"])
        w.writerows(rows)
    print(f"\n已写入 {OUT}")


if __name__ == "__main__":
    main()
