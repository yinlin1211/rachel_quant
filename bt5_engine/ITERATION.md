# bt5 迭代链路

## 2026-11-06：逐 tick 循环优化

### 背景

bt5 回测一天（20260105，篮子 1000 只）基线：

- 冷缓存：236.97 s（22 GB tick 文件首次读盘，I/O 约 170 s）
- 热缓存：65.09 s，427,688,459 条 tick，0.152 µs/条
- 80 核机器只用 1 核

### 定位

逐 tick 主循环在开源文件 `demo/swap_basket_manager.cpp::start_sim` 里，不是闭源 `.so`。基线有三处无效工作：

1. 每条 tick 都做 `symbolMap_.find(tkr_list[cidx])` 字符串哈希
2. 77.8% 的 tick（3.33 亿条）不属于篮子，仍调用 `get_md/get_ts/get_od` 取整条记录，只为读 `nTime`
3. 每条 tick 都判断 `newest_time < startTime / > endTime`

### 改动（`-DOPT_LOOP`）

| # | 改动 | 效果 |
|---|---|---|
| 1 | 预建 `algoByCidx_[cidx]` 数组 | 去掉逐 tick 字符串哈希 |
| 2 | cidx 不在篮子直接 `continue` | 非篮子 tick 不再取记录 |
| 3 | 二分定位 endTime 边界 | 去掉逐条时间比较 |

### 结果

| 指标 | 基线 | 优化后 | 变化 |
|---|---:|---:|---:|
| 循环耗时 | 65.09 s | 38.94 s | -40.2% |
| 每条 tick | 0.152 µs | 0.091 µs | -40% |
| 加速比 | — | 1.67× | 省 26.15 s/天 |

篮子 tick 数（94,981,793）和成交输出完全一致，语义未变。信号共享内存 `/bt5test` 为空，本次只隔离逐 tick 循环成本，不含撮合下单。

### 剩余瓶颈

优化后 39 s 中，约 32 s 是 95M 条篮子 tick 的：

- `las->on_tick()` + `process_trader_state()`（策略状态机）
- `pbt_->on_tick()`（闭源撮合）
- `get_md/ts/od`（O(1)，占比小）

### 下一步

- 分阶段计时（get_data / 策略 / 撮合）
- 按 symbol 分片并行（ShBook/SzBook 天然 per-symbol），需处理 `lf_market_mgr` 单例和 `Statistics` 共享状态
- 连续多日跑时用热缓存，稳定值约 39 s/天

### 复现

```bash
cd demo
g++ -std=c++11 -O2 ... -o demo_base        # 原循环
g++ -std=c++11 -O2 -DOPT_LOOP ... -o demo_opt
./demo_base --tickPath=/opt/bar_data --scoreFile=/bt5test \
  --basketFile=basket.csv --tradeDate=20260105 \
  --startTime=93000 --endTime=145700 --cuoheType=0
```
