SELECT
    date,
    cidx,
    intDiv(time, 10000000) * 60 + intDiv(time, 100000) % 100 AS mod,
    countIf(chType = 'D')                          AS cxl_cnt,
    countIf(chType IN ('0','A'))                   AS add_cnt,
    sumIf(volume, chType = 'D')                    AS cxl_vol,
    sumIf(volume, chType IN ('0','A'))             AS add_vol,
    countIf(chType = 'D' AND chBS = 'B')           AS cxl_b,
    countIf(chType = 'D' AND chBS = 'S')           AS cxl_s,
    countIf(chType IN ('0','A') AND chBS = 'B')    AS add_b,
    countIf(chType IN ('0','A') AND chBS = 'S')    AS add_s
FROM od
WHERE date IN (20260105,20260106,20260107,20260108,20260109,20260112,20260113,20260114,20260115,20260116,20260721,20261020)
GROUP BY date, cidx, mod
FORMAT CSV
