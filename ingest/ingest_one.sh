#!/bin/bash
# 单日入库：先顺序预读进页缓存，再多线程 RowBinary 直连（全部用 idle I/O 优先级）
d=$1
f=/opt/bar_data/TickPool/$d.dat
log=/opt/intern2_clickhouse/logs/ingest.log
s=$(date +%s)
ionice -c3 dd if=$f of=/dev/null bs=8M 2>/dev/null
m=$(date +%s)
if ionice -c3 /home/intern2/rachel/ingest/bin/dump_mt $f $d 16 2>/dev/null \
   | clickhouse-client --port 9100 --max_threads=8 --query "INSERT INTO od FORMAT RowBinary"; then
  echo "$d OK preload=$((m-s))s dump=$(( $(date +%s) - m ))s total=$(( $(date +%s) - s ))s" >> $log
else
  echo "$d FAIL $(date '+%F %T')" >> $log
fi
