#!/bin/bash
# 3 个月（20260721–20261020）因子评估：1,000 条模板因子
#
# 前置：panels/ 里已有这些日期的 .panel（用 run_pipeline.sh 或手动建的）
# 用法：./run_eval_3m.sh [线程数=12]
set -euo pipefail
cd "$(dirname "$0")"

THREADS=${1:-12}
DATES=$(ls /opt/bar_data/TickPool/*.dat | xargs -n1 basename | sed 's/\.dat$//' \
        | awk '$0 >= "20260721" && $0 <= "20261020"' | sort)
NDAY=$(echo "$DATES" | wc -l)
echo "days=$NDAY"

# 检查面板是否齐全
missing=""
for d in $DATES; do [ -f "panels/$d.panel" ] || missing="$missing $d"; done
if [ -n "${missing// /}" ]; then
    echo "缺少面板:$missing"
    echo "先运行：./run_pipeline.sh 20260721 20261020 /tmp/ignore 2   (或只建面板)"
    exit 1
fi

mkdir -p out_3m
echo "evaluating 1000 factors on $NDAY days (threads=$THREADS)..."
OMP_NUM_THREADS=$THREADS ./bin/alpha_expr panels template_factors.txt \
    out_3m/template_ic.csv $DATES

echo "analysing..."
python3 tools/analyze_factors.py out_3m/template_ic.csv out_3m/template_ic.csv.daily.csv \
    template_factors.txt.map template_factors.txt.catalog out_3m
