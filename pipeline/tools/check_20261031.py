#!/usr/bin/env python3
"""Repo-level QA for 2026-10-31 docs, data, and answer artifacts."""
from __future__ import annotations

import csv
from pathlib import Path
from collections import defaultdict


ROOT = Path(__file__).resolve().parents[2]


REQUIRED_DOCS = (
    "evening_cheatsheet_20261031.md",
    "factor_answer_20261031.md",
    "research_notes_20261031.md",
    "pipeline/panel_books/README.md",
    "pipeline/panel_books/data_dictionary.md",
)

REQUIRED_DATA = (
    "results/alpha101_integrity_summary_20261031.csv",
    "results/template_ic.csv",
    "results/corr_matrix.csv",
    "results/ic_stats.csv",
    "results/alpha101_value_check_20261031.csv",
    "pipeline/panel_books/event_hourly_pressure.csv",
    "pipeline/panel_books/event_hourly_pressure_daily.csv",
    "pipeline/panel_books/event_spread_period_summary.csv",
    "pipeline/panel_books/event_spread_summary.csv",
)

REQUIRED_TOOLS = (
    "pipeline/tools/alpha101_integrity.py",
    "pipeline/tools/alpha101_value_check.py",
    "pipeline/tools/event_hourly_pressure.py",
    "pipeline/tools/event_spread.py",
    "pipeline/tools/event_heatmap.py",
    "pipeline/tools/analyze_panelbook.py",
)


def iter_files(paths):
    for rel in paths:
        yield rel, ROOT / rel


def missing_and_empty(paths):
    missing, empty = [], []
    for rel, path in iter_files(paths):
        if not path.exists():
            missing.append(rel)
            continue
        if path.stat().st_size == 0:
            empty.append(rel)
    return missing, empty


def summarize_alpha(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    low_sample = sum(
        int(float(row["value_n_median"])) < 500000 for row in rows
    )
    zero_days = sum(int(row["days_zero"]) > 0 for row in rows)
    low_coverage = sum(
        float(row["published_coverage"]) < 0.8 for row in rows
    )
    return {
        "rows": len(rows),
        "days_with_values_9_or_more": sum(
            int(row["days_with_values"]) >= 9 for row in rows
        ),
        "days_with_values_10": sum(int(row["days_with_values"]) == 10 for row in rows),
        "zero_day_factors": [row["factor"] for row in rows if int(row["days_zero"])],
        "low_sample_factors": [row["factor"] for row in rows if low_sample],
        "low_coverage_factors": [row["factor"] for row in rows if low_coverage],
        "max_daily_abs_ic": max(
            abs(float(row["daily_ic_mean"])) for row in rows
        ),
    }


def summarize_pressure(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    return {
        "rows": len(rows),
        "hours": [row["hour"] for row in rows],
        "event_count_sum": sum(int(row["event_count"]) for row in rows),
        "buy_median_max": max(float(row["buy_median"]) for row in rows),
        "sell_median_max": max(float(row["sell_median"]) for row in rows),
        "buy_over_1_max": max(float(row["buy_over_1"]) for row in rows),
        "sell_over_1_max": max(float(row["sell_over_1"]) for row in rows),
    }


def summarize_daily_pressure(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    cells_by_day = defaultdict(list)
    for row in rows:
        cells_by_day[row["day"]].append(row["hour"])
    missing_day_hours = {
        day: sorted(set(("09", "10", "11", "13", "14")) - set(hours))
        for day, hours in cells_by_day.items()
        if sorted(hours) != ["09", "10", "11", "13", "14"]
    }
    return {
        "rows": len(rows),
        "days": len({row["day"] for row in rows}),
        "hours": sorted({row["hour"] for row in rows}),
        "event_count_sum": sum(int(row["event_count"]) for row in rows),
        "missing_day_hours": missing_day_hours,
        "buy_over_1_max": max(float(row["buy_over_1"]) for row in rows),
        "sell_over_1_max": max(float(row["sell_over_1"]) for row in rows),
    }


def summarize_period_summary(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    return {
        "rows": len(rows),
        "metrics": len({row["metric"] for row in rows}),
        "periods": sorted({row["period"] for row in rows}),
    }


def summarize_template(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    return {
        "rows": len(rows),
        "columns": list(rows[0].keys()) if rows else [],
    }


def summarize_corr(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    return {
        "rows": len(rows),
        "columns": list(rows[0].keys()) if rows else [],
    }


def summarize_ic_stats(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    return {
        "rows": len(rows),
        "horizons": sorted({row["horizon"] for row in rows}),
        "factors": sorted({row["factor"] for row in rows}),
    }


def summarize_value_check(path: Path):
    with path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    corr_values = []
    for row in rows:
        try:
            corr_values.append(float(row["corr_cpp_ref"]))
        except ValueError:
            pass
    return {
        "rows": len(rows),
        "shape_match_false": sum(row["shape_match"] == "False" for row in rows),
        "corr_over_099": sum(value > 0.99 for value in corr_values),
        "corr_under_080": sum(value < 0.80 for value in corr_values),
    }


def main():
    report = {}
    for category, paths in {
        "required_docs": REQUIRED_DOCS,
        "required_data": REQUIRED_DATA,
        "required_tools": REQUIRED_TOOLS,
    }.items():
        missing, empty = missing_and_empty(paths)
        report[category] = {"missing": missing, "empty": empty}

    data = ROOT / "results"
    alpha_dir = data / "alpha101_integrity_summary_20261031.csv"
    pressure_path = ROOT / "pipeline/panel_books/event_hourly_pressure.csv"
    daily_pressure_path = ROOT / (
        "pipeline/panel_books/event_hourly_pressure_daily.csv"
    )
    period_summary_path = (
        ROOT / "pipeline/panel_books/event_spread_period_summary.csv"
    )

    if alpha_dir.exists():
        report["alpha101"] = summarize_alpha(alpha_dir)
    if pressure_path.exists():
        report["hourly_pressure"] = summarize_pressure(pressure_path)
    if daily_pressure_path.exists():
        report["daily_hourly_pressure"] = summarize_daily_pressure(
            daily_pressure_path
        )
    if period_summary_path.exists():
        report["period_summary"] = summarize_period_summary(period_summary_path)

    template_ic = data / "template_ic.csv"
    corr_matrix = data / "corr_matrix.csv"
    ic_stats = data / "ic_stats.csv"
    value_check = data / "alpha101_value_check_20261031.csv"
    if template_ic.exists():
        report["template_ic"] = summarize_template(template_ic)
    if corr_matrix.exists():
        report["corr_matrix"] = summarize_corr(corr_matrix)
    if ic_stats.exists():
        report["ic_stats"] = summarize_ic_stats(ic_stats)
    if value_check.exists():
        report["value_check"] = summarize_value_check(value_check)

    def has_block(pattern: str, key: str):
        path = ROOT / pattern
        text = path.read_text(encoding="utf-8")
        return key in text

    if (ROOT / "evening_cheatsheet_20261031.md").exists():
        report["top_doc_has"] = {
            "event_doc": has_block(
                "evening_cheatsheet_20261031.md",
                "pipeline/panel_books/README.md",
            ),
            "spread_summary": has_block(
                "evening_cheatsheet_20261031.md",
                "event_spread_period_summary.csv",
            ),
            "hourly_pressure": has_block(
                "evening_cheatsheet_20261031.md",
                "event_hourly_pressure.csv",
            ),
            "factor_doc": has_block(
                "evening_cheatsheet_20261031.md",
                "factor_answer_20261031.md",
            ),
        }

    if (ROOT / "factor_answer_20261031.md").exists():
        report["factor_doc_has"] = {
            "alpha101_integrity": has_block(
                "factor_answer_20261031.md",
                "alpha101_integrity_summary_20261031.csv",
            ),
            "hourly_pressure_exists": has_block(
                "factor_answer_20261031.md",
                "event_hourly_pressure.py",
            ),
            "spread_summary_exists": has_block(
                "factor_answer_20261031.md",
                "event_spread_period_summary.csv",
            ),
        }

    has_missing = any(
        section.get("missing") for section in report.values() if isinstance(section, dict)
    )
    report["status"] = "missing_paths" if has_missing else "ok"
    print(report)


if __name__ == "__main__":
    main()
