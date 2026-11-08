#!/bin/bash
cd /home/intern2/rachel/pipeline
OUT=/home/intern2/rachel/experiments/e5_split
for d in 20260105 20260106 20260107 20260108 20260109 20260112 20260113 20260114 20260115 20260116; do
  [ -f "$OUT/od_split_$d.csv" ] && { echo "$d exists, skip"; continue; }
  echo "=== $d $(date +%H:%M:%S) ==="
  ./bin/od_split $d "$OUT/od_split_$d.csv" > "$OUT/$d.log" 2>&1
  echo "  exit=$? rows=$(wc -l < $OUT/od_split_$d.csv 2>/dev/null)"
done
echo "=== done $(date +%H:%M:%S) ==="
