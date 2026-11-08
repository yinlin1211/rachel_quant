#!/usr/bin/env python3
# 解析 bt5 tradelog 里的下单/成交/撤单，输出统一 CSV
# 用法: parse_bt5_log.py <tradelog> <out.csv>
import re, sys, csv

src = sys.argv[1]
dst = sys.argv[2]

KEY = re.compile(r"^(send new order|stock transaction|order filled|order canceled|order received|request cancel)")
KV = re.compile(r"([A-Za-z_]+):([^,]+)")

rows = []
with open(src, errors="ignore") as f:
    for line in f:
        m = re.search(r"\]\s*(.*)$", line)
        if not m:
            continue
        body = m.group(1).strip()
        km = KEY.match(body)
        if not km:
            continue
        typ = km.group(1)
        kv = dict(KV.findall(body))
        rows.append({
            "type": typ,
            "symbol": kv.get("symbol", ""),
            "side": kv.get("side", ""),
            "qty": kv.get("qty", ""),
            "prc": kv.get("prc", ""),
            "oseq": kv.get("oseq", ""),
            "reportTime": kv.get("reportTime", ""),
            "tickTime": kv.get("tickTime", ""),
            "tickPrc": kv.get("tickPrc", ""),
            "score": kv.get("score", ""),
        })

with open(dst, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["type","symbol","side","qty","prc","oseq","reportTime","tickTime","tickPrc","score"])
    w.writeheader()
    w.writerows(rows)

from collections import Counter
c = Counter(r["type"] for r in rows)
print("parsed %d rows -> %s" % (len(rows), dst))
for k, v in c.most_common():
    print("  %-20s %d" % (k, v))
