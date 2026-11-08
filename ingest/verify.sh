#!/bin/bash
# 入库最终验收：逐日行数对账 + 总量 + 磁盘
OUT=/opt/intern2_clickhouse/logs
mkdir -p $OUT/verify
echo "[1/4] 读取每天应有条数（只读计数，不解码）..."
/home/intern2/rachel/ingest/bin/count_od /opt/bar_data/TickPool/20*.dat \
  | awk '{n=split($1,p,"/"); split(p[n],a,"."); print a[1]"\t"$2}' | sort > $OUT/verify/expected.tsv

echo "[2/4] 读取 ClickHouse 每天实际行数..."
clickhouse-client --port 9100 --query \
 "SELECT partition, sum(rows) FROM system.parts WHERE table='od' AND active GROUP BY partition ORDER BY partition FORMAT TSV" \
 | sort > $OUT/verify/actual.tsv

echo "[3/4] 对账..."
join -t $'\t' $OUT/verify/expected.tsv $OUT/verify/actual.tsv > $OUT/verify/joined.tsv
awk -F'\t' '$2!=$3{print "MISMATCH "$1" expected="$2" actual="$3; bad++} END{printf "days=%d mismatched=%d\n", NR, bad+0}' $OUT/verify/joined.tsv | tee $OUT/verify/result.txt

echo "[4/4] 总量与磁盘..."
clickhouse-client --port 9100 --query "SELECT concat('rows=', toString(count()), ' dates=', toString(uniqExact(date))) FROM od" | tee -a $OUT/verify/result.txt
clickhouse-client --port 9100 --query "SELECT concat('disk=', formatReadableSize(sum(bytes_on_disk))) FROM system.parts WHERE table='od' AND active" | tee -a $OUT/verify/result.txt
