#!/bin/bash
cd /home/intern2/rachel/pipeline
for p in 0.005 0.01 0.05 0.10; do
  echo "=== participation=$p $(date +%H:%M:%S) ==="
  SIM_PARTICIPATION=$p ./bin/sim_live 20260105 /home/intern2/rachel/experiments/e6_impact/p${p} 20 60 0 0 \
      > /home/intern2/rachel/experiments/e6_impact/p${p}.log 2>&1
  echo "  exit=$?"
done
echo "=== 全部完成 $(date +%H:%M:%S) ==="
