#!/bin/bash
# 补录：20250609、20250731、20250801（源文件 2026-11-02 补齐）
# 流程同 backfill_days.sh：dd 预读 -> dump_mt 导出 -> 字节校验 -> INSERT -> 行数对账
set -u
BIN=/home/intern2/rachel/ingest/bin
TMP=/home/intern2/backfill_tmp
LOG=/opt/intern2_clickhouse/logs/backfill_20261103.log
CH="clickhouse-client --port 9100"
mkdir -p "$TMP"
DAYS="20250609 20250731 20250801"

echo "START $(date '+%F %T')" >> "$LOG"
for d in $DAYS; do
  f=/opt/bar_data/TickPool/$d.dat
  if [ ! -f "$f" ]; then echo "$d NO_FILE $(date '+%T')" >> "$LOG"; continue; fi
  n=$($CH -q "SELECT count() FROM od WHERE date=$d")
  if [ "$n" != "0" ]; then echo "$d SKIP existing=$n $(date '+%T')" >> "$LOG"; continue; fi

  s=$(date +%s)
  ionice -c3 dd if="$f" of=/dev/null bs=8M 2>/dev/null
  m=$(date +%s)

  exp=$($BIN/count_od "$f" | awk '{print $2}')
  $BIN/dump_mt "$f" "$d" 16 2>/dev/null > "$TMP/$d.bin"
  rc=$?
  bytes=$(stat -c %s "$TMP/$d.bin" 2>/dev/null || echo 0)
  want=$((exp*24))
  if [ "$rc" -ne 0 ] || [ "$bytes" != "$want" ]; then
    echo "$d DUMP_FAIL rc=$rc bytes=$bytes want=$want $(date '+%T')" >> "$LOG"
    rm -f "$TMP/$d.bin"; continue
  fi

  $CH --max_threads=8 -q "INSERT INTO od FORMAT RowBinary" < "$TMP/$d.bin"
  irc=$?
  got=$($CH -q "SELECT count() FROM od WHERE date=$d")
  if [ "$irc" -ne 0 ] || [ "$got" != "$exp" ]; then
    echo "$d INSERT_FAIL irc=$irc got=$got exp=$exp -> DROP PARTITION $(date '+%T')" >> "$LOG"
    $CH -q "ALTER TABLE od DROP PARTITION '$d'"
  else
    echo "$d OK rows=$got preload=$((m-s))s since_preload=$(( $(date +%s)-m ))s total=$(( $(date +%s)-s ))s" >> "$LOG"
  fi
  rm -f "$TMP/$d.bin"
done
echo "ALL_DONE $(date '+%F %T')" >> "$LOG"
