# Panelbook / event-data dictionary

## 1. `panelbook`

Panelbook 是“每分钟 × 每只股票”的盘口快照，固定行宽，适合直接读二进制/做统计。

## 2. `*panelbook.events.csv`

| 字段 | 含义 | 单位 |
|---|---|---|
| `minute` | 事件分钟 `HHMMSS` | — |
| `cidx` | 股票索引 | — |
| `event_z` | 3 分钟收益 / 自身前 20 样本波动 | 无单位倍数 |
| `close` / `open` / `high` / `low` | 分钟 OHLC | 元 |
| `amount` | 分钟成交额 | 元 |
| `spread_bp` | (ask - bid) / mid × 10000 | bp |
| `ask_depth` / `bid_depth` | L1 股数价格 | 股 |
| `ask_notional` / `bid_notional` | L1 名义资金 | 元 |
| `ask_l1_depth_share` / `bid_l1_depth_share` | L1 股数 / 全簿股数 | 比例 |
| `ask_l1_notional_share` / `bid_l1_notional_share` | L1 名义资金 / 全簿名义资金 | 比例 |
| `limit_up` / `limit_down` | 价格上下限 | 元 |
| `buy_volume` / `sell_volume` | 分钟主动买卖股数 | 股 |
| `buy_notional` / `sell_notional` | 分钟主动买卖金额 proxy | 万元 |

## 3. `event_hourly_pressure.csv`

| 字段 | 含义 | 单位 |
|---|---|---|
| `hour` | 事件小时 09 / 10 / 11 / 13 / 14 | — |
| `event_count` | 同小时内 buy/sell 双侧 ratio 都可算的事件行数 | 行 |
| `buy_median` / `sell_median` | 同小时 active flow / first-layer notional 中位数 | 倍 |
| `buy_over_1` / `sell_over_1` | 同小时内比值 >1 的行占比 | 比例 |
| `daily_buy_median` / `daily_sell_median` | 先按天取中位，再跨 12 日取中位 | 倍 |

## 4. `event_hourly_pressure_daily.csv`

| 字段 | 含义 | 单位 |
|---|---|---|
| `day` / `hour` | 交易日内的小时条目，不是单独一天平均 | — |
| `event_count` | 该小时内买/卖双侧 ratio 都可算的事件行数 | 行 |
| `buy_median` / `sell_median` | 该小时内 active flow / first-layer notional 中位数 | 倍 |
| `buy_over_1` / `sell_over_1` | 该小时内比值 >1 的行占比 | 比例 |

20260129 的 `09` 桶是分日最弱的一档：buy median `11.12x`，sell median `49.81x`，超一档比例 `91.0% / 98.5%`；跨日小时表会把这个极端分日压低。

## 5. `event_spread_summary.csv`

每行是 `panel × period × metric`，输出 `p25/p50/p75`。可用于同一天内不同时间窗口的同期对比。

## 6. `event_spread_period_summary.csv`

跨日 summary，把每个 `period × metric` 汇总。`cross_day_median` 是先按日取中位，再跨 12 日取中位；`cross_day_mean` 是同口径的平均。

## 7. 事件 / 主动流 / 盘口三层，别混用

1. **价格可进**：`spread_bp`；
2. **一档资金**：`ask_notional` / `bid_notional`；
3. **主动流压力**：`buy_notional` / `sell_notional` 与 `buy_volume` / `sell_volume`。

前两都是盘口数据；第三是主动成交数据。`buy_notional / ask_notional` 是“同侧资金/一档资金”的 proxy。

## 8. 一句话避免常见陷阱

`buy_notional / ask_notional` 不是“下单上限”；它是“事件分钟同侧主动流 / 当前一档名义深度”的压力 proxy，不能等同于“能吃多少股”。
