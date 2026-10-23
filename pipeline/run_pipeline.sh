#!/bin/bash
# Alpha101 全流程：TickPool -> 分钟面板 -> 82 个因子 -> IC 统计
#
# 用法:
#   ./run_pipeline.sh <起始日期> <结束日期> <输出目录> [并发数]
#   ./run_pipeline.sh 20260105 20260116 out_10d 2
#
# 说明:
#   - 面板已存在则跳过（增量构建），删掉 panels/<date>.panel 即强制重建
#   - 并发数默认 2：实测该机械盘单进程已打满 ~240MB/s，3 路以上无收益反而增加争抢
#   - 只重新计算因子时，可跳过第 1 步直接跑 bin/alpha101
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
START="${1:?usage: $0 <start YYYYMMDD> <end YYYYMMDD> <out_dir> [jobs]}"
END="${2:?usage: $0 <start YYYYMMDD> <end YYYYMMDD> <out_dir> [jobs]}"
OUT="${3:?usage: $0 <start YYYYMMDD> <end YYYYMMDD> <out_dir> [jobs]}"
JOBS="${4:-2}"

BIN="$ROOT/bin"
PANELS="$ROOT/panels"
LOGS="$ROOT/logs"
mkdir -p "$OUT" "$PANELS" "$LOGS"

DATES=$(ls /opt/bar_data/TickPool/*.dat 2>/dev/null \
        | xargs -n1 basename | sed 's/\.dat$//' \
        | awk -v s="$START" -v e="$END" '$0 >= s && $0 <= e' | sort)
[ -z "$DATES" ] && { echo "no dates between $START and $END"; exit 1; }
NDAY=$(echo "$DATES" | wc -l)
echo "dates ($NDAY): $(echo $DATES)"

# ---------- 1. 建面板（缺哪个建哪个） ----------
TODO=""
for d in $DATES; do [ -f "$PANELS/$d.panel" ] || TODO="$TODO $d"; done
if [ -n "${TODO// /}" ]; then
    echo "building panels (jobs=$JOBS):$TODO"
    build_one() {
        d="$1"; s=$(date +%s)
        "$BIN/panel_tool" "$d" "$PANELS" > "$LOGS/$d.log" 2>&1
        echo "$d elapsed=$(( $(date +%s) - s ))s" >> "$LOGS/panel_timing.txt"
    }
    export -f build_one; export BIN PANELS LOGS
    printf '%s\n' $TODO | xargs -P "$JOBS" -I{} bash -c 'build_one {}'
else
    echo "panels already built, skip"
fi

# ---------- 2. 因子计算 ----------
echo "computing factors..."
"$BIN/alpha101" "$PANELS" "$OUT" $DATES

# ---------- 3. IC 汇总（只读 ic_stats.csv，几 KB） ----------
echo "summarising IC..."
python3 "$ROOT/tools/summarize_ic.py" "$OUT" 1

# ---------- 4. 可选：从 .dat 重算一遍做交叉验证 ----------
# python3 "$ROOT/tools/analyse_a101.py" "$PANELS" "$OUT" "$(echo $DATES | tr ' ' ',')"
