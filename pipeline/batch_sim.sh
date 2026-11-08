#!/bin/bash
# 批量逐 tick 模拟：batch_sim.sh <dates_file> <hold> <out_dir> <worker_idx> <nworkers>
set -u
cd "$(dirname "$0")"
LIST=$1; HOLD=$2; OUT=$3; W=$4; NW=$5
mkdir -p "$OUT"
i=0
while read -r D; do
  [ -z "$D" ] && continue
  i=$((i+1))
  [ $(( (i-1) % NW )) -ne "$W" ] && continue
  ./bin/sim_live "$D" "$OUT/$D" 20 "$HOLD" 50 0 > "$OUT/$D.log" 2>&1
  echo "$D,$HOLD,$?" >> "$OUT/results_$W.csv"
done < "$LIST"
echo "worker $W finished"
