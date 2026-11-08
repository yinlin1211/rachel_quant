# 实验报告：bt5 闭源撮合引擎接入验证与结果复现

**日期**：2026-11-08
**标的日**：20260105（单日全市场）
**目的**：验证 `liblf_bt5.so` 接入链路打通，并复现 2026-11-06 的历史结果
**结论**：✅ 链路打通，**结果逐位复现**（成交量/成交额/利润三项完全一致）

---

## 1. 一句话结论

bt5 引擎此前"仅评估、未接入"，本次打通了完整链路，并在相同口径下**逐位复现**历史结果
（成交率 98.05%、成交额占比 97.08%、利润 +0.36 BP）。两组不同阈值配置的结果差异已定位为
**参数差异而非 bug**。

---

## 2. 链路

```
分钟面板 (.panel)
   │  gen_scores.py        score = 3分钟收益 / 过去20分钟1分钟收益标准差 (z值)
   ▼
scores_20260105.csv        symbol,HHMMSSmmm,score
   │  make_signals         映射到全局 tick 下标（单遍扫描 TickPool）
   ▼
signals_20260105.csv       didx,tidx,cidx,score
   │  make_signal_shm      写入共享内存
   ▼
/dev/shm/bt5sig            trade_signal[] {didx,tidx,cidx,score}
   │  demo (liblf_bt5)     策略增量读取 shm，逐 tick 撮合
   ▼
tradelog + 汇总行          volume/amount/profit
```

规模：**134,355 个信号、994 只股票、436,852,698 条 tick**。

---

## 3. 三个必须知道的格式（此前卡住的原因）

| 项 | 正确形式 | 错误形式的后果 |
|---|---|---|
| `--basketFile` | `TKR,QTY` 表头 + `000012.SZ,22500` | 只给代码列表 → 每行 `error line` |
| `--scoreFile` | **共享内存名**（`/bt5sig`） | 给 CSV 路径 → `failed to open shm` |
| `didx` / `tidx` | **都是全局 tick 下标**（`TickPool.get_data(k)` 的 k） | 造假值 → `real_thread` 内崩溃（栈回溯无符号） |

引擎还从**当前工作目录**读取 `scoreThreshold.csv`（每股买入/卖出阈值），这是两组结果差异的根源。

---

## 4. 两组配置与结果

同一份信号、同一份 basket，只改阈值文件：

| 配置 | 阈值文件 | 成交量 | 成交率 | 成交额占比 | 利润 | 利润(BP) |
|---|---|---|---|---|---|---|
| **A：示例阈值** | `bt5_demo/scoreThreshold.csv`（5104 行，各股不同） | 6,079,751 | 71.88% | 70.66% | **-23,897.14** | **-3.24** |
| **B：固定 ±1.0** | `bt5_ab/threshold_pm1.csv`（1000 行） | 8,293,312 | **98.05%** | **97.08%** | **+3,646.29** | **+0.36** |

分母：8,458,300 股 / 104,378,078.95 元。

> **阈值含义**：score 为 z 值，`> buyThres` 买、`< sellThres` 卖。
> 配置 A 的阈值按个股历史波动标定（例如 000001.SZ 为 ±4.7），信号很难触发，故成交率低；
> 配置 B 统一 ±1.0，触发更频繁。

---

## 5. 与历史结果的一致性验证（关键）

配置 B 与 2026-11-06 那次运行的输出**逐位相同**：

| 指标 | 2026-11-06 | 2026-11-08（本次） | 一致 |
|---|---|---|---|
| volume | 8,293,312 / 8,458,300 (98.05%) | 8,293,312 / 8,458,300 (98.05%) | ✅ |
| amount | 101,325,679.37 / 104,378,078.95 (97.08%) | 101,325,679.37 / 104,378,078.95 (97.08%) | ✅ |
| profit | 3,646.29 (0.36 BP) | 3,646.29 (0.36 BP) | ✅ |
| 订单数 | 3,257（`bt5_orders_20260105.csv`） | **3,257**（tradelog `order received`） | ✅ |

**这坐实了链路可复现**——不是"能跑起来"，而是"同输入同输出"。

---

## 6. 运行明细（配置 B）

| 项 | 值 |
|---|---|
| 引擎耗时 | 15:22:39 → 15:26:07 ≈ **3 分 28 秒**（全量 4.37 亿 tick） |
| 订单回报 | 3,257 笔 |
| 撤单 | 237 次 |
| 异常回报 | 79 条 `ERROR pending_cancel A`（见 §7） |
| tradelog | 17,290 行 / 2.7 MB |

---

## 7. 已知问题

### 7.1 `ERROR pending_cancel A`（79 条）

策略在「撤单请求在途（`ORD_PENDING_CANCEL`）」状态收到「新增确认（`A`）」，示例策略的
`recv_report_state_pending_cancel` 只处理了 `C`（撤单成功）与 `J`（拒绝），未处理 `A`，
于是打 ERROR 日志。**不影响撮合结果**（该订单后续仍被正常处理），属示例代码瑕疵。

修法：在 `bt5_demo/lf_algo_stock.cpp` 的 `recv_report_state_pending_cancel` 增加 `case 'A'` 分支。
本次未改，保持与历史运行可比。

### 7.2 本次未产出逐笔订单 CSV

历史运行的 `bt5_orders_20260105.csv` / `bt5_events_20260105.csv` 由额外解析脚本产生
（`parse_bt5_log.py`）。本次只保留了 stdout 汇总与 tradelog，订单明细需重跑解析：

```sh
cd bt5_ab && python3 parse_bt5_log.py ../bt5_demo/tradelog.txt_2026-11-08.txt > orders.csv
```

> tradelog 因体积（2.7 MB）未入库，`.gitignore` 已排除 `bt5_demo/tradelog.txt_*`。

### 7.3 引擎不校验信号时序的合理性

把全部信号一次性推入 shm、引擎按 `tidx` 顺序消费时，信号会"提前到达"。
本次沿用历史做法（`make_signals` 把每个信号挂到时间≥目标的 tick 上），语义正确。

---

## 8. 复现步骤

```sh
cd /home/intern2/rachel/bt5_ab

# ① 面板 → 分数
python3 gen_scores.py /home/intern2/rachel/pipeline/panels/20260105.panel \
        scores_20260105.csv /home/intern2/rachel/bt5_demo/basket.csv

# ② 分数 → 全局 tick 下标
./make_signals 20260105 scores_20260105.csv signals_20260105.csv \
   /home/intern2/rachel/bt5_demo/basket.csv

# ③ 写入共享内存
./make_signal_shm /bt5sig 1000000 signals_20260105.csv

# ④ 跑引擎（注意 cwd 决定读哪份 scoreThreshold.csv）
cd /home/intern2/rachel/bt5_demo
cp /home/intern2/rachel/bt5_ab/threshold_pm1.csv scoreThreshold.csv   # 配置B
./demo --tickPath=/opt/bar_data --scoreFile=/bt5sig \
       --basketFile=$(pwd)/basket.csv --tradeDate=20260105
```

**期望输出（配置 B）**：
```
volume:8293312/8458300(98.05%), amount:101325679.37/104378078.95(97.08%), profit:3646.29(0.36BP)
```

编译依赖与坑见 `MIGRATION.md` §5.3。

---

## 9. 与自研引擎的对照（下一步）

本次只验证了 bt5 自身可复现。与 `sim_live`（自研逐 tick 模拟）的**同信号 A/B 对比**尚未做：

- `sim_live`：按量比例成交 + 订单生命周期 + T+1 + 费用，单日 1 分 44 秒
- `bt5`：闭源真实订单簿（`ShBook`/`SzBook`），单日 3 分 28 秒

两者成交率、逆向选择成本、净收益的差异，正是开题报告 **E4（做市模拟：快照假设 vs 订单级精确）**
要回答的问题。历史已有 `compare_ab.py` / `compare_ab2.py` 与 `sim_replay*` 产物，
但口径未统一，需要在同一 basket、同一信号、同一时间段下重跑。

---

## 10. 文件索引

| 文件 | 说明 |
|---|---|
| `bt5_ab/gen_scores.py` | 面板 → 分数 |
| `bt5_ab/make_signals.cpp` | 分数 → 全局 tick 下标（**信号语义的权威实现**） |
| `bt5_ab/make_signal_shm.cpp` | 写入共享内存 |
| `bt5_ab/threshold_pm1.csv` | 配置 B 的阈值（固定 ±1.0） |
| `bt5_ab/bt5_orders_20260105.csv` | 历史订单明细（2026-11-06） |
| `bt5_ab/bt5_events_20260105.csv` | 历史事件明细（2026-11-06） |
| `bt5_ab/parse_bt5_log.py` | tradelog → 订单/事件 CSV |
| `bt5_demo/scoreThreshold.csv` | 示例阈值（配置 A，5104 行） |
| `bt5_demo/tradelog.txt_*` | 运行日志（**不入库**，体积大） |
