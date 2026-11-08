#!/bin/bash
cd /home/intern2/rachel/pipeline
mkdir -p /home/intern2/rachel/experiments/e6_impact_big
for p in 0.005 0.01 0.05 0.10; do
  echo "=== equity=1e8 participation=$p $(date +%H:%M:%S) ==="
  SIM_EQUITY=100000000 SIM_PARTICIPATION=$p \
    ./bin/sim_live 20260105 /home/intern2/rachel/experiments/e6_impact_big/p${p} 20 60 0 0 \
    > /home/intern2/rachel/experiments/e6_impact_big/p${p}.log 2>&1
  echo "  exit=$?"
done
echo "=== 完成 $(date +%H:%M:%S) ==="
