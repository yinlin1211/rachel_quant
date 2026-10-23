# Alpha101 因子计算流水线

把 TickPool 的逐笔 L2 数据压成分钟面板，再在面板上批量计算 Alpha101 因子并做 IC 统计。
全流程 C++ 计算，Python 只做统计分析。

## 目录结构

```
pipeline/
├── build.sh              # 编译到 bin/
├── run_pipeline.sh       # 一键跑全流程
├── src/
│   ├── common.h          # Mat、截面/时序算子（rank/scale/ts_*/decay/...）
│   ├── alphas.cpp        # 82 条 Alpha101 公式 + 名称表
│   ├── main.cpp          # 加载面板 -> 并行算因子 -> 写 .mkt/.dat
│   └── panel_tool.cpp    # TickPool -> 分钟面板
├── tools/
│   ├── summarize_ic.py   # 从 ic_stats.csv 汇总 IC（最终结果，几十毫秒）
│   └── analyse_a101.py   # 从 .dat 重算 IC（备用验证，慢但独立）
├── bin/                  # 编译产物（panel_tool, alpha101）
├── panels/               # 分钟面板，60 MB/天
├── out_10d/              # 因子输出 .mkt/.dat
└── logs/                 # 每天建面板的日志与耗时
```

## 数据流

```
/opt/bar_data/TickPool/YYYYMMDD.dat        23.9 GB/天，4.37 亿条 tick
        │  panel_tool：顺序扫一遍，按「股票 × 分钟」归并
        ▼
panels/YYYYMMDD.panel                       60 MB/天，48 字节/行
        │  alpha101：载入全部日期 -> 82 条公式并行计算
        ▼
out/YYYYMMDD.mkt   每分钟每股 4×int32（时间, 分钟, 股票, 中间价）
out/YYYYMMDD.dat   每分钟每股 82×float32（因子值）
out/alphaInfo.txt  股票数, 因子数, 因子名
        │  analyse_a101.py
        ▼
out/result_alpha101.csv   IC均值/std/IR/IC>0占比/合并IC/覆盖率
```

### 面板行格式（PanelRow，48 字节）

| 字段 | 类型 | 含义 |
|---|---|---|
| nTime | int32 | 该分钟首条 MD 的时间（HHMMSSmmm） |
| minute | int32 | 分钟（HHMMSS） |
| cidx | int32 | 股票在 tkr.txt 中的下标 |
| nMd / nTs | int32 | 该分钟 MD / TS 条数（诊断用） |
| open/high/low/close | float | 中间价（买一+卖一)/2 的分钟 OHLC |
| vwap | float | 成交额 / 成交量 |
| volume / amount | float | TS 累加的成交量 / 成交额 |

### 面板构建规则

- 时间槽固定 240 个：09:30–11:29（0–119）、13:00–14:59（120–239）
- 集合竞价、午休、收盘后丢弃；最后 500 个 tick 丢弃（与平台口径一致）
- 每只股票每天输出 240 行；缺失分钟用上一分钟 close 前向填充，开盘段用后一分钟回填
- 全天无数据的股票保留 NaN（20260105 有 46 只）

## 实测性能（10 天，20260105–20260116）

| 阶段 | 耗时 | 说明 |
|---|---|---|
| 建面板（5 路） | **1228 s** | 239 GB tick → 600 MB；只做一次 |
| 载入面板 | 1.1 s | 600 MB |
| 82 因子计算（32 线程） | 11.8 s | 内存带宽受限，加线程收益递减 |
| IC 统计（内存内） | **1.3 s** | 直接累加 Σx/Σy/Σxy/Σx²/Σy²，不落盘回读 |
| 写盘（按天并行） | 16.5 s | 4.1 GB，约 245 MB/s（磁盘上限） |
| **计算阶段合计** | **30 s** | |
| IC 汇总（读 ic_stats.csv） | **<0.1 s** | 只读 275 KB |
| 从 `.dat` 重算做验证 | 6 s | 可选，结果与内存统计逐位相同 |

输出体积：`.dat` 410 MB/天 + `.mkt` 20 MB/天，10 天共 4.1 GB。

单日建面板：热缓存 50 s，冷读且并发争抢时 337–934 s。

### IC 结果的两份来源

| 文件 | 来源 | 用途 |
|---|---|---|
| `ic_stats.csv` | C++ 在内存中直接累加 | **最终结果**，275 KB，汇总耗时 <0.1 s |
| `YYYYMMDD.dat` + `.mkt` | 因子值落盘 | **备用验证**，可用 `analyse_a101.py` 重算比对 |

`ic_stats.csv` 每行是一个 `(日期, 因子, 周期)` 的 Pearson 充分统计量：

```
date,factor,horizon,n,sx,sy,sxy,sxx,syy
```

IC 由此精确重建（与逐值计算相比误差为 0，已实测验证）：

```
cov = n·Σxy − Σx·Σy
vx  = n·Σx² − (Σx)²
vy  = n·Σy² − (Σy)²
IC  = cov / sqrt(vx·vy)
```

同一份统计量既能得到逐日 IC，也能跨日求和得到合并 IC。

### 并发怎么选

磁盘上限约 **240 MB/s**，单进程就能打满：

| 路数 | 实测合计吞吐 |
|---|---|
| 1 | 232 MB/s |
| 3 | 229 MB/s |
| 5 | 245 MB/s（但总耗时 1228 s，比单路理论值多 23%） |

**结论：默认 2 路。** 3 路以上不增加吞吐，只增加争抢和内存压力。
真正的提速手段是「只读一次」：面板建好后，再算多少因子都不用碰 TickPool。

## 使用方法

```bash
./build.sh                                   # 编译
./run_pipeline.sh 20260105 20260116 out_10d 2
```

只改因子、不重建面板：

```bash
OMP_NUM_THREADS=16 bin/alpha101 panels out_new 20260105 ... 20260116
python3 tools/summarize_ic.py out_new 1          # 几分钟内出 IC
```

交叉验证（从 `.dat` 重算，应得到完全相同的 IC）：

```bash
python3 tools/analyse_a101.py panels out_new 20260105,20260106,... 10
```

## 关于 101 条公式

实现了 **82 条**。未实现的 19 条（48/56/58/59/63/67/69/70/76/79/80/82/87/89/90/91/93/97/100）
依赖**行业分类**（IndClass）或**市值**（cap），本数据没有对应字段。
这与 KunQuant 官方 `predefined/Alpha101.py` 的 `all_alpha` 列表完全一致。

公式文本以该文件为参考，但实现是独立的（我们自己的 C++），未使用其计算引擎。

## 已知限制

1. **尚未做数值校验**：82 条公式与参考实现的逐值对比还没做，目前只验证了能跑通、输出格式正确。
2. **分钟级适配**：Alpha101 原本定义在日线上，这里用分钟 bar（10 天拼接成 2400 步），
   长窗口（200/230/250）会跨越隔夜跳空。
3. **不支持半日市**：固定 240 槽，若某日只有上午交易，下午会被前向填充。
4. **面板字段是固定集合**：新因子若需要逐笔委托方向、撤单、深度等，需扩展 PanelRow 并重建面板。
5. **`.mkt` 的 nTime 是分钟对齐的**（minute×1000），不是原始 tick 时间戳。
