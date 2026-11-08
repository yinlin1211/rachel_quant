# bt5 回测引擎包

lf_basket_trade 的 bt5 回测/撮合引擎，含闭源动态库、公开头文件和一个可编译的 demo 策略。

> 闭源库（`liblf_bt5.so` / `liblquant_kernel.so` / `liblf_utillq.so`）属主为 `yhl`，仅限内部研发使用，不要对外分发。

## 目录

```
bt5_engine/
├── lib/           预编译动态库（自研 3 个 + 第三方 zmq/mysql/crypto++ 等）
├── include/       公开头文件（策略基类、撮合基类、tick pool、日志）
├── demo/          示例策略工程（可编译）
│   ├── swap_basket_manager.cpp/h   篮子换仓策略，逐 tick 主循环在这里
│   ├── lf_algo_stock.cpp/h         单票下单状态机
│   ├── main.cpp                    入口，调用 main_impl(create_strategy, ...)
│   ├── config.xml                  时间/下单参数
│   ├── basket.csv                  篮子（TKR,QTY）
│   └── scoreThreshold.csv          每票买卖阈值
└── bench/         基准输出（优化前后）
```

## 引擎分层

| 层 | 文件 | 职责 |
|---|---|---|
| 策略插件 | `demo/swap_basket_manager.cpp` | 派生 `quant_strategy_base`，导出 `create_strategy()` |
| 撮合插件 | `liblf_bt5.so` 内 `quant_backtrace_base` | `init / on_request / on_tick / calc` |
| 引擎 | `liblf_bt5.so` | `main_impl`、订单簿（ShBook/SzBook）、Statistics、ConfigManager |
| 数据内核 | `liblquant_kernel.so` | `lquant_tick_pool`（mmap + 全局偏移索引） |
| 工具 | `liblf_utillq.so` | 杂项 |

策略与撮合通过 `set_backtrace()` 和请求/回报回调对接，但执行流仍是单线程。

## 编译

```bash
cd demo
g++ -std=c++11 -O2 \
  -I ../include -I ../include/kernel -I ../include/include \
  swap_basket_manager.cpp lf_algo_stock.cpp main.cpp -o demo \
  -lrt -L../lib -Wl,-rpath,$PWD/../lib \
  -llf_utillq -llf_bt5 -llquant_kernel
```

加 `-DOPT_LOOP` 编译优化版（见 `ITERATION.md`）。

## 运行

```bash
./demo \
  --tickPath=/opt/bar_data \
  --scoreFile=/bt5test \
  --basketFile=basket.csv \
  --tradeDate=20260105 \
  --startTime=93000 --endTime=145700 --cuoheType=0
```

- `tickPath` 是 TickPool 的父目录，引擎自己拼 `/TickPool/<date>.dat`
- `scoreFile` 是共享内存名（信号），不是 CSV；阈值仍读当前目录的 `scoreThreshold.csv`
- 需要 `/opt/bar_data/Meta/tkr.txt` 存在

## 基准（20260105 全天，篮子 1000 只）

| 指标 | 基线 | 优化后 |
|---|---:|---:|
| 循环耗时（热缓存） | 65.09 s | 38.94 s |
| 每条 tick | 0.152 µs | 0.091 µs |
| 总 tick | 427,688,459 | 427,688,458 |
| 篮子 tick | 94,981,793 | 94,981,793 |

冷缓存首跑约 237 s（22 GB 文件从盘读，I/O 约 170 s）。

详见 `ITERATION.md`。
