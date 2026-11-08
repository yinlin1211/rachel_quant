# Panelbook / event-data 速查

## 1. 这批文件是什么

Panelbook 是“每分钟 × 每只股票”的盘口快照，已经把 L2 原始行情压成固定长度的二进制面板。
开头是 240 分钟槽位 × 全市场股票，后面是每只股票一条事件行。
Eevent-output CSV 是同一批数据的可读版，方便直接看事件面压力。

## 2. 三个层级，不要混着读

1. **Panelbook 二进制**：盘口结构、主动买卖、挂撤、OFI 同步都存在单文件里。
2. **`*panelbook.events.csv`**：事件后一分钟的盘口与主动成交对照，可以直接做热图 / 统计。
3. **`event_hourly_pressure.csv`**：小时级 buy/sell 压力，用来回答“能不能吃第一档”的 proxy。

## 2.1 一张表看依据

| 想知道什么 | 优先看哪份 | 说清什么口径 |
|---|---|---|
| **价差是否变宽** | `pipeline/panel_books/event_spread_period_summary.csv` | `spread_bp` 的 `same → after_1/3/5/10/15/30/45/60` |
| **一档资金是否变薄** | `pipeline/panel_books/event_spread_period_summary.csv` | `ask_notional` / `bid_notional`，以及 `ask/bid_l1_*_share` |
| **主动买卖压力（跨日）** | `pipeline/panel_books/event_hourly_pressure.csv` | `buy/sell_notional / buy/sell_volume`，以及 first-layer ratio |
| **主动买卖压力（分日）** | `pipeline/panel_books/event_hourly_pressure_daily.csv` | 同一压力口径的 `day × hour` 明细，不要和跨日中位混读 |
| **是不是极限压力** | 同上 | `buy_over_1` / `sell_over_1` |
| **小时段是否等权** | 同上 | 无； 小时段是事件密度加权，不是全天等权 |
| **数据结构和单位** | `pipeline/panel_books/data_dictionary.md` | active-flow 与 first-layer notional 的单位与限制 |
| **因子侧应该答什么** | `factor_answer_20261031.md` | 82 Alpha101 与 1,000 模板因子分开，低覆盖单独提 |
| **今晚速答手册** | `evening_cheatsheet_20261031.md` | 不重复数据含义，只给口径和答法 |

## 3. 事件文件列名

每个 `*panelbook.events.csv` 都按以下顺序输出：

| 字段 | 含义 |
|---|---|
| `minute` | 事件分钟 `HHMMSS` |
| `cidx` | 股票索引 |
| `event_z` | 3 分钟收益 / 自身前 20 样本波动|
| `close` / `open` / `high` / `low` | 分钟 OHLC |
| `amount` | 分钟成交额 |
| `spread_bp` / `depth_share` | 价差 / 一档占总深度比值 |
| `ask_depth` / `bid_depth` | L1 股数 |
| `ask_notional` / `bid_notional` | L1 name元金额 |
| `limit_up` / `limit_down` | 价格上下限 |
| `buy_volume` / `sell_volume` | 分钟主动买卖股数 |
| `buy_notional` / `sell_notional` | 分钟主动买卖金额 proxy（万元） |
| `buy/ask_L1` | 主动买入金额 / L1 卖方名义资金 |
| `sell/bid_L1` | 主动卖出金额 / L1 买方名义资金 |

不要把 `ask_notional` 和 `buy_notional` 当同层，它们是两类数据。

## 4. 事件工具

```bash
python pipeline/tools/event_spread.py pipeline/panel_books/20260129.panelbook
python pipeline/tools/event_hourly_pressure.py pipeline/panel_books/20260129.panelbook --output /tmp/hour.csv
python pipeline/tools/event_heatmap.py pipeline/panel_books/20260129.panelbook.events.csv --step 5
```

`event_hourly_pressure.py` 是**小时桶压力指标**，不是全天等权。
`event_spread.py` 是**跨时间同期指标**，etrofit 事件后 1/3/5/10/15/30/45/60 分钟。

## 4.1 `event_hourly_pressure.csv`

| 字段 | 含义 |
|---|---|
| `hour` | 事件小时：09 / 10 / 11 / 13 / 14 |
| `event_count` | 同一小时内可计算 buy/sell 双侧 ratio 的事件行数 |
| `buy_median` / `sell_median` | 事件分钟主动流资金 / 一档名义资金的中位值 |
| `buy_over_1` / `sell_over_1` | 同一小时内比值 >1 的行占比 |
| `daily_buy_median` / `daily_sell_median` | 先按天取中位，再跨 12 日取中位 |

解读时先说小时段，再说“事件密度加权”，最后才是中位值。
不要把 `09/10/13/14` 小时段直接当成等权全天平均。

## 4.2 `event_hourly_pressure_daily.csv`

| 字段 | 含义 |
|---|---|
| `day` | 交易日，最新 12 面板共 60 个 `day × hour` 非空行 |
| `hour` | 交易日内的 5 个事件密度加权小时桶 |
| `event_count` | 该日该小时里买/卖双侧 ratio 都可算的事件行数 |
| `buy_median` / `sell_median` | 该日该小时的同向压力中位数 |
| `buy_over_1` / `sell_over_1` | 该日该小时比值大于 1 的占比 |

先读本表再看跨日表；跨日中位会隐藏 20260203/20260204 这类早段压力更薄、更极端的分日差异。

## 4.3 `event_spread_summary.csv`

每行是 `panel × period × metric`，输出 `p25/p50/p75`。适合同时按日看同一因子的压力形状。

## 4.4 `event_spread_period_summary.csv`

这里是跨日 summary，把每个 `period × metric` 汇总：

| 字段 | 含义 |
|---|---|
| `period` | same / after_1 / after_3 / after_5 / after_10 / after_15 / after_30 / after_45 / after_60 |
| `metric` | spread_bp / ask_depth / bid_depth / ask_notional / bid_notional / depth_share / active flow |
| `day_count` | 有值的交易日数 |
| `cross_day_median` | 先按日取中位，再跨日取中位 |
| `cross_day_mean` | 同一圈跨日 average |
| `day_p25` / `day_p75` | 跨日第一/第三四分位 |

这个文件回答“事件后 1 分钟/3 分钟/10 分钟不同窗口的同期指标”，不是小时级。

## 5. 一个简短的回答示例

若问“这个事件能不能吃第一档”，先看 three layer：

1. spread: `spread_bp`.
2. 一档资金：`ask_notional/bid_notional`.
3. 主动流：`buy_notional/sell_notional`.

然后报口径：**第一层不是下单上限，只是“事件分钟同侧主动流 / 一档名义深度”的 proxy**。

## 6. 已知限制

1. `buy_notional / sell_notional` 是主动成交 proxy，不是真正的执行上限。
2. L1 depth share 是第一层占全部盘口的份额，不代表绝对金额。
3. 涨跌停不是布尔位，过滤要按 `close >= limit_up` 或 `close <= limit_down`。
4. 小时段是事件密度加权，不能直接说“全天都这样”。
