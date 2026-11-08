#!/bin/bash
# ofi_cnt 单月试跑：tick 级计算 -> 日频截面 IC
# 区间 2026-09-01 ~ 2026-09-30（约 21 个交易日）
set -u
OUT=/home/intern2/rachel/ofz
CH=http://127.0.0.1:8124/
D1=20260901
D2=20260930
F=$OUT/trial_raw_${D1}_${D2}.csv
COLS="date,cidx,
 countIf(chBS='B') AS nB,
 countIf(chBS='S') AS nS,
 sumIf(volume,chBS='B') AS vB,
 sumIf(volume,chBS='S') AS vS,
 sumIf(price*volume,chBS='B') AS amtB,
 sum(price*volume) AS amtAll"

s=$(date +%s)
curl -s --max-time 1800 "$CH" --data-binary \
  "SELECT $COLS FROM od WHERE date>=$D1 AND date<=$D2 GROUP BY date,cidx FORMAT CSV" > "$F"
rc=$?
e=$(date +%s)
echo "agg rc=$rc rows=$(wc -l < "$F") wall=$((e-s))s at $(date '+%F %T')"
python3 -I "$OUT/trial_ic.py" "$F"
