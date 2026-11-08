#!/usr/bin/env python3
"""Value-check Alpha101 output against the independent Pandas reference."""
from __future__ import annotations

import argparse
import os
import sys
import time
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import pandas as pd


ROOT = Path(__file__).resolve().parents[2]
PANEL_ROW_DTYPE = np.dtype([
    ("nTime", "<i4"), ("minute", "<i4"), ("cidx", "<i4"), ("nMd", "<i4"), ("nTs", "<i4"),
    ("open", "<f4"), ("high", "<f4"), ("low", "<f4"), ("close", "<f4"),
    ("vwap", "<f4"), ("volume", "<f4"), ("amount", "<f4"),
])
PANDAS_NAMES = {
    "open": "S_DQ_OPEN", "high": "S_DQ_HIGH", "low": "S_DQ_LOW",
    "close": "S_DQ_CLOSE", "volume": "S_DQ_VOLUME", "amount": "S_DQ_AMOUNT",
}


def load_alpha_names(out_dir: Path):
    text = (out_dir / "alphaInfo.txt").read_text()
    first, *lines = text.strip().splitlines()
    _, count = first.split(",")
    names = [line.strip() for line in lines]
    if len(names) != int(count):
        raise ValueError("alpha names do not match header")
    return names


def load_panel_arrays(panel_dir: Path, date: int):
    path = panel_dir / f"{date}.panel"
    rows = np.fromfile(path, dtype=PANEL_ROW_DTYPE)
    if len(rows) != rows["cidx"].shape[0] or len(rows) % 240:
        raise ValueError("panel row count is not a multiple of 240")
    stocks = len(rows) // 240
    arrays = {}
    for field, name in PANDAS_NAMES.items():
        values = np.ascontiguousarray(rows[field], dtype="<f4")
        arrays[name] = pd.DataFrame(values.reshape(240, stocks).astype("float64"))
    return arrays


def _reference_process(job):
    index, factor_name, arrays = job
    import importlib.util
    source = ROOT / "kunquant/tests/KunTestUtil/ref_alpha101.py"
    spec = importlib.util.spec_from_file_location("alpha101_reference", source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    alphas = module.Alphas({k: v.copy() for k, v in arrays.items()})
    reference = getattr(alphas, factor_name)()
    required = ["open", "close", "volume"]
    if any(field not in arrays for field in PANDAS_NAMES.values()):
        return index, None
    return index, reference


def compare_factor(factor: str, cpp: np.ndarray, reference: pd.DataFrame):
    ref = reference.to_numpy(dtype="float64")
    values = cpp.astype("float64")
    valid = np.isfinite(ref) & np.isfinite(values)
    if not valid.any():
        return {"factor": factor, "shape_match": ref.shape == values.shape,
                "reference_nonfinite_zero": int(np.sum(~np.isfinite(ref)))}
    abs_diff = np.abs(ref - values)
    scale = np.maximum(np.abs(ref), np.abs(values))
    relative = np.divide(abs_diff, scale, out=np.full_like(ref, np.nan), where=scale > 1e-20)
    offset = float(np.median((ref - values)[valid]))
    corrected_values = values + offset
    corrected_valid = valid & np.isfinite(corrected_values)
    corrected_abs = np.abs(ref - corrected_values)
    corrected_scale = np.maximum(np.abs(ref), np.abs(corrected_values))
    corrected_rel = np.divide(
        corrected_abs, corrected_scale, out=np.zeros_like(corrected_abs),
        where=scale > 1e-20,
    )
    corrected_abs = corrected_abs[corrected_valid]
    corrected_rel = corrected_rel[corrected_valid]
    values_flat = values[valid].ravel()
    ref_flat = ref[valid].ravel()
    correlation = np.corrcoef(values_flat, ref_flat)[0, 1] if values_flat.size > 1 else 0.0
    return {
        "factor": factor,
        "shape_match": ref.shape == values.shape,
        "value_count": int(valid.sum()),
        "reference_nonfinite_zero": int(np.sum(~np.isfinite(ref))),
        "cpp_nonfinite_zero": int(np.sum(~np.isfinite(values))),
        "max_abs_diff": float(np.max(abs_diff[valid])),
        "mean_abs_diff": float(np.mean(abs_diff[valid])),
        "median_abs_diff": float(np.median(abs_diff[valid])),
        "p99_abs_diff": float(np.quantile(abs_diff[valid], 0.99)),
        "offset_median_cpp_minus_ref": offset,
        "raw_bad_count": int(np.sum(abs_diff[valid] > 1e-4)),
        "raw_bad_rate": float(np.mean(abs_diff[valid] > 1e-4)),
        "offset_bad_count": int(np.sum(
            corrected_abs > 1e-4 if corrected_scale[corrected_valid].size else corrected_scale.shape[0]
            if corrected_scale > 1e-20 else (corrected_scale > 1e-20)
        ) if corrected_valid.any() else 0),
        "offset_bad_rate": float(np.mean(
            corrected_rel > 1e-5 if corrected_rel.size > 0 else corrected_scale.shape[0]
        ) if corrected_valid.any() else 0.0),
        "offset_p99_abs_diff": float(np.quantile(corrected_abs, 0.99)) if corrected_valid.any() else np.nan,
        "corr_cpp_ref": float(correlation),
    }


def run(panel_dir: Path, out_dir: Path, date: int, output_path: Path, workers: int,
        selected_factors=None):
    names = load_alpha_names(out_dir)
    arrays = load_panel_arrays(panel_dir, date)
    factor_data = np.memmap(out_dir / f"{date}.dat", dtype="<f4", mode="r")
    rows = factor_data.reshape(240, arrays["S_DQ_OPEN"].shape[1], len(names))
    submitted = [(index, name, arrays) for index, name in enumerate(names)
                 if selected_factors is None or name in selected_factors]
    started = time.time()
    results = {}
    partial = []
    with ThreadPoolExecutor(max_workers=workers) as pool:
        for index, reference in pool.map(_reference_process, submitted):
            name = names[index]
            results[name] = compare_factor(name, rows[:, :, index], reference)
            partial.append(results[name])
            elapsed = time.time() - started
            print(f"{name} {len(partial)}/{len(names)} elapsed={elapsed:.1f}s", flush=True)
    frame = pd.DataFrame([results[name] for name in names
                          if selected_factors is None or name in selected_factors])
    frame.to_csv(output_path, index=False)
    return frame


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("panel_dir")
    parser.add_argument("out_dir")
    parser.add_argument("date", type=int)
    parser.add_argument("--output", required=True)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--factors", default="")
    args = parser.parse_args()
    frame = run(
        Path(args.panel_dir), Path(args.out_dir), args.date, Path(args.output), args.workers,
        selected_factors=tuple(args.factors.split(",")) if args.factors else None,
    )
    print(frame.head().to_string(index=False))


if __name__ == "__main__":
    main()
