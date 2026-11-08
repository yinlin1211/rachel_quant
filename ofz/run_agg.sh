#!/bin/bash
# 从 ClickHouse od 表按 日期×股票 聚合订单流原始统计量
# 实测：4 路并行会把磁盘打成随机读(120MB/s)，单路顺序读约 490MB/s，所以这里串行跑
set -u
OUT=/home/intern2/rachel/ofz
CH=http://127.0.0.1:8124/
COLS="date,cidx,
 countIf(chBS='B') AS nB,
 countIf(chBS='S') AS nS,
 sumIf(volume,chBS='B') AS vB,
 sumIf(volume,chBS='S') AS vS,
 countIf(chType='D') AS nD,
 countIf(chType='D' AND chBS='B') AS nDB,
 countIf(chType='D' AND chBS='S') AS nDS,
 sumIf(volume,chType='D') AS vD,
 countIf(price*volume>=5000000000) AS nBig,
 sumIf(volume,price*volume>=5000000000) AS vBig,
 sumIf(volume,time>=143000000) AS vLate,
 sumIf(price*volume,chBS='B') AS amtB,
 sum(price*volume) AS amtAll"

curl -s "$CH" --data-binary "SELECT DISTINCT partition_id FROM system.parts WHERE active AND table='od' ORDER BY partition_id FORMAT TSV" > $OUT/dates.txt
N=$(wc -l < $OUT/dates.txt)
echo "dates=$N start $(date '+%F %T')" >> $OUT/agg_progress.log
awk -v n=$N 'BEGIN{k=int((n+3)/4)} {a[NR]=$1} END{for(i=0;i<4;i++){s=i*k+1;e=(i+1)*k; if(e>n)e=n; if(s<=n) print s,e,a[s],a[e]}}' $OUT/dates.txt > $OUT/chunks.txt

while read -r _s _e d1 d2; do
  [ -z "${d2:-}" ] && continue
  f=$OUT/raw_${d1}_${d2}.csv
  if [ -s "$f" ]; then echo "skip $d1-$d2 (exists)" >> $OUT/agg_progress.log; continue; fi
  s0=$(date +%s)
  curl -s --max-time 7200 "$CH" --data-binary \
    "SELECT $COLS FROM od WHERE date>=$d1 AND date<=$d2 GROUP BY date,cidx FORMAT CSV" > "$f"
  echo "chunk $d1-$d2 rows=$(wc -l < $f) wall=$(( $(date +%s)-s0 ))s at $(date '+%T')" >> $OUT/agg_progress.log
done < $OUT/chunks.txt

echo "ALL_DONE $(date '+%F %T')" >> $OUT/agg_progress.log
cat $OUT/raw_*.csv > $OUT/raw_all.csv
echo "merged rows=$(wc -l < $OUT/raw_all.csv) at $(date '+%T')" >> $OUT/agg_progress.log
