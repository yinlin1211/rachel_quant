#!/bin/bash
# 逐 tick 模拟实盘：单日一跑
# 用法：./run_sim.sh <YYYYMMDD> [out_dir] [topN] [hold] [latency_ms]
set -e
cd "$(dirname "$0")"
D=${1:?date}
OUT=${2:-out_sim}
TOPN=${3:-20}
HOLD=${4:-10}
LAT=${5:-50}
mkdir -p "$OUT"
bin/sim_live "$D" "$OUT/$D" "$TOPN" "$HOLD" "$LAT" 0
