#!/bin/bash
cd /home/intern2/rachel/pipeline
OUT=/home/intern2/rachel/experiments/rq1_l3
for d in 20260105 20260106; do
  for spec in "A D" "0 D" "0A D"; do
    set -- $spec
    tag="add$1_cxl$2"
    echo "=== $d $tag $(date +%H:%M:%S) ==="
    timeout 600 ./bin/validate_l3 $d 0935 0940 $1 $2 > $OUT/${d}_${tag}.txt 2>&1
    echo "  exit=$?"
  done
done
echo "=== 完成 $(date +%H:%M:%S) ==="
