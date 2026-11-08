# PROGRESS

## 2026-10-23 22:34 — 环境确认
- `liblquant_kernel.so` 导出 `lquant_tick_pool::{load,get_md_count,get_md,get_ts_count,get_ts,...}`。
- 结构体定义在 `lquantTick.hpp`：`lquant_market_data_t=TDFDefine_MarketData`（256B）、
  `lquant_transaction_t=TDFDefine_TransactionEx`（32B）。
- 链接时需要额外 `-ldl`，并自行定义 `LQUANTSELFLog::logger`（库只引用未定义）。

## 2026-10-23 22:35 — 最小探针通过
- `/tmp/fastprobe/probe`：`pool.load("/opt/bar_data/TickPool/20260105.dat")` 返回 1，
  `last_didx=436,852,698`，与 README 记录的总条数一致。
- cidx 0 的 md=4832、ts=70960；面板中 cidx 0 的 nMd 合计 4760、nTs 合计 69603，
  差值来自 15:00 收盘集合竞价（面板只保留 09:30–11:29 / 13:00–14:59）。
- 确认 cidx 与 `/opt/bar_data/Meta/tkr.txt`（5213 行）顺序一致。

## 2026-10-23 22:36 — 面板口径核对
- 阅读 `/home/intern2/rachel/pipeline/src/panel_tool.cpp`：
  - OHLC = MD 买卖一档中间价（单边缺失取另一侧，双边<=0 丢弃）
  - volume/amount = TS 的 nVolume/nTurnover
  - 240 槽；缺失分钟前向填充，开盘段用首个有效分钟回填
  - 行序：slot 外层 × cidx 内层；48B packed
- 对账脚本 `tools/reconcile.py` 已写好（整数精确比较 + 浮点位模式比较）。

## 2026-10-23 22:37 — fast_reader 编译并启动
- `src/fast_reader.cpp` 编译通过（见 README 中的完整链接命令）。
- 22:37 启动 `./out/fast_reader 20260105 out`，日志 `out/fast_reader.log`。

## 2026-10-23 22:38 — 新读取器首跑完成
- `out/fast_reader.log`：
  - md=21,470,995；ts=186,411,462；读取总条数 207,882,457
  - load=0.000s，md=8.235s，ts=28.349s，fill=0.003s，write=0.100s
  - **total=36.739s**，5.66 M 条/秒
  - 缺失分钟填充 950 个；无数据股票 46 只（46×240=11,040 行 NaN）
- 输出 `out/20260105.fast.panel`，60,053,760 字节，与参考面板同尺寸。

## 2026-10-23 22:39 — 对账 100% 一致
- `python3 -I tools/reconcile.py` 结果写入 `out/reconcile_20260105.txt`：
  - 1,251,120 / 1,251,120 行完全一致（整数相等 + 浮点位模式相等）
  - 12 个字段最大绝对误差全部为 0，bad_rows 全部为 0
  - sum_nMd=21,071,668、sum_nTs=184,612,075、sum_volume/amount 均与参考完全相等
- 原因：逐条复刻 panel_tool.cpp 的口径（MD 中间价、TS 量额、240 槽、前向填充、slot×cidx 行序）。

## 2026-10-23 22:40 — 旧读法同机基准
- 运行 `pipeline/bin/panel_tool 20260105 /tmp/oldpanel`（get_tick 全量顺序扫描 + 分桶），
  用于给 READER.md 提供同机、同日期、同缓存状态的对比数字。

## 2026-10-23 22:43 — 旧路径同机基准结果（重要）
- `pipeline/bin/panel_tool 20260105 /tmp/oldpanel`：**wall=47.757s**，
  nTicks=436,852,698，missing_filled=950。
- 输出 md5 与 `pipeline/panels/20260105.panel`、`out/20260105.fast.panel` 三者相同
  （e5696d03b199228c0dd7aa4493fe2f9e）。
- 172 秒基线在本机当前二进制上无法复现；`pipeline/logs/panel_timing.txt` 中
  20260105 也是 50s。判断 172s 属于更早的“全量排序 + 落中间列”实现。
- 结论修正：新读法 36.74s，相对当前旧读法 47.76s 为 **1.30×**；
  相对文档 172s 基线为 4.68×。两个口径在 READER.md 中分开写明，不混用。

## 2026-10-23 22:44 — READER.md 完成
- 已写：两套读法、实测耗时、对账 100%、口径解释、接入建议、复现命令。
- 交付物：src/fast_reader.cpp、tools/reconcile.py、out/20260105.fast.panel、
  out/fast_reader.log、out/reconcile_20260105.txt、READER.md、PROGRESS.md。

## 2026-10-23 22:44 — 复跑验证
- `out/fast_reader 20260105 /tmp/fastrun2`：38.61s（md=8.030s，ts=30.425s），
  输出 md5 与首跑相同（e5696d03...），耗时波动约 ±5%。

## 2026-10-23 22:45 — 最终交付清单
- `src/fast_reader.cpp`：可编译可运行，按 cidx 获取 MD/TS 并聚合 240 分钟面板。
- `tools/reconcile.py`：逐字段/逐位对账，输出 `out/reconcile_20260105.txt`。
- `out/20260105.fast.panel`：36.74s 生成，与参考面板 100% 逐位一致。
- `out/fast_reader.log`：条数与分阶段耗时。
- `READER.md`：结论、两套耗时口径、对账、接入建议、复现命令。
- 未修改 `pipeline/` 下任何文件；旧路径基准只读运行，输出写到 `/tmp/oldpanel`。
