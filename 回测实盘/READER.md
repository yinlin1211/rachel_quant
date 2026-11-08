# 按股票索引读取 TickPool：实测与对账

日期：2026-10-23
样本：`/opt/bar_data/TickPool/20260105.dat`（23.9 GB，436,852,698 条）
参考：`/home/intern2/rachel/pipeline/panels/20260105.panel`（60,053,760 B，1,251,120 行）

---

## 1. 结论

| 读法 | 单日耗时 | 全年 237 天 | 相对文档基线 |
|---|---:|---:|---:|
| 旧 A（文档基线）：`get_tick` 全量扫描 + 全量排序 | 172 s（既有记录） | 11.3 h | 1.00× |
| 旧 B（本次同机复现）：`pipeline/bin/panel_tool`，`get_tick` 扫描 + 直接分桶 | **47.76 s** | 3.14 h | 3.60× |
| **新：`lquant_tick_pool` 按 cidx 随机访问（`out/fast_reader`）** | **36.74 s** | **2.42 h** | **4.68×** |

- 新读取器单日 **36.74 秒**，达到 45 秒目标；读取 207,882,457 条 MD+TS，5.66 M 条/秒。
- 与现有分钟面板对账：**1,251,120 / 1,251,120 行逐位一致（100%）**，
  12 个字段最大绝对误差全部为 **0**，成交量/成交额总量完全相等。
- 结论：**可以接入**。接入时保留 `tools/reconcile.py` 作为每日回归闸门。

> **关于 172 秒基线的重要更正**：本次在同一台机器上无法复现 172 秒。
> 当前 `pipeline/bin/panel_tool`（就是生成参考面板的那个二进制）跑同一天只需
> **47.76 秒**，且输出与参考面板 md5 完全相同；`pipeline/logs/panel_timing.txt`
> 里 20260105 的历史记录也是 `elapsed=50s`。
> 因此 172 秒对应的应是更早的“全量排序 + 落中间列”实现（当前 bin 中已不存在），
> 而**新读法相对当前旧读法的实际增益是 1.30×（47.76 → 36.74 秒），不是 4.7×**。
> 4.68× 只能表述为“相对文档中的 172 秒基线”。

---

## 2. 两套读法

### 2.1 旧读法

```c
init_tick_file("/opt/bar_data", date);
int n = get_tick_count();          // 4.37 亿
for (int k = 0; k < n; ++k) {
    TickResult t = get_tick(k);    // 全局顺序，每次返回 (cidx, type, ptr)
    ...
}
```

`get_tick(k)` 按全库顺序逐条返回，下游必须先完整扫描、再按 `(cidx, minute)` 分桶
（`panel_tool` 用大小为 `5213×240` 的数组直接分桶；更早的实现是先落中间文件再排序）。
无论哪种，代价都是 **4.37 亿次函数调用 + 全量数据流过内存**。

### 2.2 新读法

```cpp
lquant_tick_pool pool;
pool.load("/opt/bar_data/TickPool/20260105.dat");   // mmap，0.000 s
for (int c = 0; c < N; ++c) {
    for (int i = 0; i < pool.get_md_count(c); ++i)
        lquant_market_data_t* md = pool.get_md(c, i);
    for (int i = 0; i < pool.get_ts_count(c); ++i)
        lquant_transaction_t* ts = pool.get_ts(c, i);
}
```

文件内每只股票的 MD/TS 子流本身就按时间有序，按 `cidx` 取出的序列直接就是时间序，
**不需要任何排序**；边读边聚合进 `5213×240` 的分钟槽，最后一次性写出。

> 说明：任务描述中的 `lquant_data_api::load_tickpool(int)` 需要先构造带
> `XmlNode` 配置的单例（还要加载 meta/daily 数据）。本次直接用同一个库导出的
> `lquant_tick_pool::load(path)`，它就是这个单例内部使用的底层加载器，
> 少一层配置依赖，行为一致（`get_last_didx()` 返回 436,852,698，与文件头总条数吻合）。

---

## 3. 实测耗时

机器：80 核 / 251 GB 内存；`/opt` 单块 ext4；同一日期、同一进程、单线程。

### 3.1 新读取器（`out/fast_reader.log`）

| 阶段 | 耗时 |
|---|---:|
| `pool.load()`（mmap） | 0.000 s |
| MD 读取 + OHLC 聚合（21,470,995 条） | 8.235 s |
| TS 读取 + 量额聚合（186,411,462 条） | 28.349 s |
| 缺失分钟填充 | 0.003 s |
| 写 60 MB 面板 | 0.100 s |
| **合计** | **36.739 s** |

- 读取条数：207,882,457（MD+TS）；吞吐 **5.66 M 条/秒**。
- 复跑一次（输出到 `/tmp/fastrun2`）：**38.61 s**（md 8.030 s + ts 30.425 s），
  输出 md5 与首跑完全相同，耗时波动约 ±5%。
- 实际触碰数据约 11.5 GB（MD 256 B × 21.47 M + TS 32 B × 186.41 M），
  等效带宽约 **312 MB/s**。
- OD（228,828,545 条）本任务不需要，未读取；若撮合需要订单流，可再加一路
  `get_od`，按同样密度估算增加约 10–15 秒（未实测）。

### 3.2 旧读取器（同机、同日期、同一份文件）

```bash
timeout 285 /home/intern2/rachel/pipeline/bin/panel_tool 20260105 /tmp/oldpanel
# 实测 wall = 47.757 s
# panel date=20260105 nTicks=436852698 rows=1251120 stocks=5213 missing_filled=950
```

产物与参考面板、新读取器输出三者 md5 相同：

```
e5696d03b199228c0dd7aa4493fe2f9e  /tmp/oldpanel/20260105.panel
e5696d03b199228c0dd7aa4493fe2f9e  pipeline/panels/20260105.panel
e5696d03b199228c0dd7aa4493fe2f9e  out/20260105.fast.panel
```

**为什么不是 172 秒？**

| 证据 | 数值 |
|---|---|
| 本次 `panel_tool` 同机复现 | 47.76 s |
| `pipeline/logs/panel_timing.txt` 中 20260105 | 50 s |
| 同文件其他日期（2 路并发） | 337–934 s |

`panel_tool.cpp` 用 `get_tick(k)` 全局扫描，但**没有显式排序**——它直接按
`cidx`、`slot` 写进 `5213×240` 数组。推测 172 秒对应的是更早的
“全量扫描 + 全量排序 + 落中间列”实现，该实现已不在当前 `bin/` 中
（`tick2cols` 仍保留排序，但输出的是逐 tick 列文件，口径与分钟面板不同，
本次没有拿它当基线）。

所以本文给出两个对比口径：

- **相对文档基线 172 s**：36.74 s，4.68×；
- **相对当前实际在用的旧读法（47.76 s）**：36.74 s，**1.30×**。

两个数字都成立，但不能混用。新读法确实把单日压到 45 秒以内，这一点不依赖基线选择。

---

## 4. 对账结果

命令：

```bash
python3 -I tools/reconcile.py
# 完整输出：out/reconcile_20260105.txt
```

| 指标 | 结果 |
|---|---|
| 行数 | 1,251,120 vs 1,251,120 |
| 完全一致行数 | **1,251,120（100.000000%）** |
| 整数字段一致行数 | 1,251,120（100%） |
| 价格 NaN 行 | 11,040（= 46 只无数据股票 × 240 分钟，两侧一致） |
| sum(nMd) | 21,071,668 vs 21,071,668，差 0 |
| sum(nTs) | 184,612,075 vs 184,612,075，差 0 |
| sum(volume) | 438,085,174,203 vs 438,085,174,203，差 0 |
| sum(amount) | 2,517,861,349,777 vs 2,517,861,349,777，差 0 |

各字段最大绝对误差与不一致行数：

| 字段 | 最大绝对误差 | 不一致行数 |
|---|---:|---:|
| nTime | 0 | 0 |
| minute | 0 | 0 |
| cidx | 0 | 0 |
| nMd | 0 | 0 |
| nTs | 0 | 0 |
| open | 0 | 0 |
| high | 0 | 0 |
| low | 0 | 0 |
| close | 0 | 0 |
| vwap | 0 | 0 |
| volume | 0 | 0 |
| amount | 0 | 0 |

不一致样本：**无**。浮点比较用的是位模式（`struct.pack('<f')` 后再比 `uint32`），
不是 `1e-6` 容差；结构体是 packed、无 padding，12 个字段恰好覆盖 48 字节，
因此字段级逐位一致**等价于逐字节一致**。两个文件的 md5 也相同
（`e5696d03b199228c0dd7aa4493fe2f9e`）。

---

## 5. 为什么能 100% 对齐

新读取器逐条复刻了 `pipeline/src/panel_tool.cpp` 的口径：

1. **cidx 顺序**：`/opt/bar_data/Meta/tkr.txt` 的 5213 行顺序，与 TickPool 的 channel 顺序一致。
2. **OHLC**：只用 MD；买卖一档都 `<= 0` 的丢弃；否则取中间价
   （单边缺失取另一侧），`open/high/low/close` 按到达顺序更新。
3. **成交量/成交额**：只用 TS 的 `nVolume` / `nTurnover`，`vwap = amount / volume`。
4. **时间槽**：`nTime`（HHMMSSmmm）截到分钟，只保留 09:30–11:29、13:00–14:59，共 240 槽；
   15:00 收盘集合竞价与盘前数据全部排除。
5. **缺失分钟**：首个有效分钟之前的槽用首分钟收盘回填；之后的空槽用前收盘前向填充；
   填充行的 `nMd/nTs` 保持 0，`nTime = slot_to_minute(slot)*1000`。
6. **行序**：分钟外层、股票内层；48 字节 packed 结构。
7. `panel_tool` 还有一处保护性截断 `limit = nTicks - 500`（丢掉全库最后 500 条）。
   本次逐位 100% 一致，说明这 500 条落在 15:00 之后，不影响 240 个交易时段；
   新读取器不需要这个截断，因为时间槽过滤已经把它们排除了。

原始条数与面板条数的差额也正好由第 4 条解释：

| 流 | TickPool 全量 | 面板使用 | 差额（盘前/15:00 后） |
|---|---:|---:|---:|
| MD | 21,470,995 | 21,071,668 | 399,327 |
| TS | 186,411,462 | 184,612,075 | 1,799,387 |

因为过滤后逐位相等，可以排除“字段口径差异、股票集合不同、排序不同”这三类问题。

---

## 6. 接入建议

### 6.1 替换范围

| 文件 | 现状 | 建议 |
|---|---|---|
| `pipeline/src/panel_tool.cpp` | `init_tick_file` + `get_tick` 全量扫描 | 改为 `lquant_tick_pool` 按 cidx 读取 |
| `pipeline/src/tick2cols.cpp` | 同上，另写 panelbook | 同上；OD 列按需加 `get_od` |
| `pipeline/src/sim_live.cpp` | 逐 tick 撮合 | 按持仓股票列表 `get_od/get_ts/get_md` 定向回读，不再扫全库 |

### 6.2 编译/链接注意事项（实际踩到的坑）

任务给出的编译命令还缺两项，否则链接失败：

```bash
g++ -O2 -std=c++11 -o out/fast_reader src/fast_reader.cpp \
    -I/opt/datas/lf_basket_trade/include \
    -I/opt/datas/lf_basket_trade/include/kernel \
    -L/opt/datas/lf_basket_trade/lib \
    -llquant_kernel -ldl -lpthread \
    -Wl,-rpath,/opt/datas/lf_basket_trade/lib
```

1. `liblquant_kernel.so` 引用了 `dlopen/dlsym`，必须加 `-ldl`。
2. 库引用了 `LQUANTSELFLog::logger` 但没有定义，使用方必须提供：

```cpp
#include "lquant_log.hpp"
std::shared_ptr<lquantlog::logger> LQUANTSELFLog::logger;
// main 里：
LQUANTSELFLog::logger = lquantlog::stdout_color_mt("console");
```

3. 运行时需要 `-Wl,-rpath,...`（或 `LD_LIBRARY_PATH`）指向
   `/opt/datas/lf_basket_trade/lib`。

### 6.3 接入后的验收

- 每接入一个新日期，先跑 `out/fast_reader <date> out`，再跑
  `python3 -I tools/reconcile.py out/<date>.fast.panel pipeline/panels/<date>.panel`；
- 要求：行数相同、`identical_rows == compared_rows`、各字段 `max_abs_err == 0`；
- 若不是 100%，先查 240 槽时间窗和 `tkr.txt` 顺序，再查字段口径。

### 6.4 性能预期

- 单日 36.7 s，237 天约 **2.42 小时**（不含下游因子计算）。
- 单线程即可；`/opt` 是单块盘，多进程并发读反而更慢（既有实测：4 路并发 245 MB/s < 单流 530 MB/s）。
- 页缓存会影响绝对值：本次两套读法是在同一台机器、先后紧邻运行，缓存条件接近；
  冷缓存绝对值未实测。按 11.5 GB / 530 MB/s 估算，纯磁盘下限约 22 秒，仍有 45 秒余量。

### 6.5 风险

- 依赖闭源 `liblquant_kernel.so` 的 ABI 与 `lquantTick.hpp` 的 packed 结构；
  升级库后必须重跑对账。
- 本次只验证了 20260105 一个交易日；建议先用 `tools/reconcile.py` 批量跑 5–10 天再全量切换。
- 46 只无数据股票在两侧都是 NaN，下游若用 `NaN != NaN` 的比较需注意。

---

## 7. 复现

```bash
cd /home/intern2/rachel/回测实盘
g++ -O2 -std=c++11 -o out/fast_reader src/fast_reader.cpp \
    -I/opt/datas/lf_basket_trade/include \
    -I/opt/datas/lf_basket_trade/include/kernel \
    -L/opt/datas/lf_basket_trade/lib -llquant_kernel -ldl -lpthread \
    -Wl,-rpath,/opt/datas/lf_basket_trade/lib
timeout 290 ./out/fast_reader 20260105 out
timeout 180 python3 -I tools/reconcile.py
```

产物：

| 文件 | 说明 |
|---|---|
| `src/fast_reader.cpp` | 按 cidx 读取 + 分钟聚合 + 写面板 |
| `tools/reconcile.py` | 逐字段/逐位对账 |
| `out/20260105.fast.panel` | 新读法输出（60,053,760 B） |
| `out/fast_reader.log` | 耗时与条数 |
| `out/reconcile_20260105.txt` | 对账完整输出 |
| `PROGRESS.md` | 过程记录 |
