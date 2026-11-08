#!/bin/bash
LOG=/opt/intern2_clickhouse/logs/ingest.log
for d in $(ls /opt/bar_data/TickPool/20*.dat | sed 's#.*/##;s#\.dat##'); do
  grep -q "^$d OK" $LOG || echo $d
done | xargs -P 2 -n 1 /home/intern2/rachel/ingest/ingest_one.sh
echo "ALL_DONE $(date '+%F %T')" >> $LOG
