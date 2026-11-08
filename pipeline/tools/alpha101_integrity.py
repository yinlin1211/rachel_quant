#!/usr/bin/env python3
"""Summarize 82 Alpha101 result integrity from the existing IC statistics."""
import argparse
import os

import numpy as np
import pandas as pd


def reconstruct_daily_ic(row):
    n = row["n"]
    covariance = n * row["sxy"] - row["sx"] * row["sy"]
    x_variance = n * row["sxx"] - row["sx"] ** 2
    y_variance = n * row["syy"] - row["sy"] ** 2
    denominator = np.sqrt(x_variance * y_variance)
    if not (covariance and x_variance > 0 and y_variance > 0):
        return np.nan
    return covariance / denominator


def summarize(input_dir, output_path):
    ic = pd.read_csv(os.path.join(input_dir, "ic_stats.csv"))
    result = pd.read_csv(os.path.join(input_dir, "result_alpha101.csv")).set_index("name")
    corr_path = os.path.join(input_dir, "corr_matrix.csv")
    corr = pd.read_csv(corr_path, index_col=0) if os.path.exists(corr_path) else None

    rows = []
    for factor, group in ic.groupby("factor"):
        horizon_rows = group[group["horizon"] == 1]
        valid = horizon_rows[horizon_rows["n"] > 0].copy()
        valid["daily_ic"] = valid.apply(reconstruct_daily_ic, axis=1)
        returned = result.loc[factor] if factor in result.index else pd.Series(dtype=float)
        max_abs_corr = np.nan
        if corr is not None and factor in corr.index and factor in corr.columns:
            max_abs_corr = corr.loc[factor].drop(index=factor).abs().max()
        rows.append(
            {
                "factor": factor,
                "days_with_values": len(valid),
                "days_zero": len(horizon_rows) - len(valid),
                "value_n_min": valid["n"].min() if len(valid) else 0,
                "value_n_median": valid["n"].median() if len(valid) else 0,
                "value_n_max": valid["n"].max() if len(valid) else 0,
                "daily_ic_mean": valid["daily_ic"].mean(),
                "daily_ic_std": valid["daily_ic"].std(ddof=0),
                "daily_ic_pos_ratio": np.mean(valid["daily_ic"] > 0),
                "max_abs_corr": max_abs_corr,
                "published_ic_mean": returned.get("IC_mean", np.nan),
                "published_ic_pooled": returned.get("IC_pooled", np.nan),
                "published_coverage": returned.get("coverage", np.nan),
                "published_days_pos": returned.get("IC_pos_ratio", np.nan),
            }
        )

    output = pd.DataFrame(rows)
    output["rank_by_abs_ic"] = output["daily_ic_mean"].abs().rank(
        ascending=False, method="first"
    ).astype(int)
    output["flag_zero_days"] = output["days_zero"] > 0
    output["flag_low_sample"] = output["value_n_median"] < 500000
    output["flag_low_coverage"] = output["published_coverage"] < 0.80
    output["rank_by_abs_ic"] = output["rank_by_abs_ic"].astype(int)
    output = output.sort_values("rank_by_abs_ic")
    output.to_csv(output_path, index=False)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_dir")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    summary = summarize(args.input_dir, args.output)
    with pd.option_context("display.max_columns", None, "display.width", 220):
        print(summary.head(20).to_string(index=False))
        print("\nzero-day factors:")
        print(summary[summary["flag_zero_days"]].to_string(index=False))
        print("\nlow-sample factors (<500k median values):")
        print(summary[summary["flag_low_sample"]].head(20).to_string(index=False))


if __name__ == "__main__":
    main()
