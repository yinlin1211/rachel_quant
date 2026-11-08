# MIGRATION：把整套实验环境迁移到新账号

本文档面向「在新账号（如 `account11`）下接管本项目的同事/AI」。
目标：**代码与流程在仓库里，数据在磁盘上，按本文档 30 分钟内恢复可运行状态。**

> 最后更新：2026-11-08（由 intern2 账号整理）

---

## 1. 这个项目是什么

A 股 Level-2 逐笔行情上的因子研究与执行评估平台。核心设计原则：

> **原始 tick 只扫一遍，落成分钟面板；之后所有实验只读面板。**

四层结构（详见 `chapter/02` 与 `pipeline/README.md`）：

| 层 | 作用 | 入口 |
|---|---|---|
| L0 | 数据读取、面板构建、口径冻结 | `pipeline/bin/panel_tool` |
| L1 | 大规模因子测试（IC/RankIC/FDR） | `pipeline/bin/alpha_expr` |
| L2 | 单模型与组合 | `pipeline/tools/*.py` |
| L3 | 策略回测与执行评估 | `pipeline/bin/backtest`（分钟级）、`pipeline/bin/sim_live`（逐 tick） |

另有一条**对照链路**：闭源撮合引擎 `liblf_bt5.so`，用于和自研 `sim_live` 做 A/B（见 §6）。

---

## 2. 仓库里有什么 / 没有什么

### 有（本仓库）

```
pipeline/            C++ 源码 + 构建脚本 + Python 工具 + 文档
  src/               panel_tool / alpha_expr / backtest / sim_live / validate_l3 ...
  tools/             IC 汇总、对账、列转 parquet
  bin/               【不进仓库】编译产物，需本地 make
bt5_ab/              bt5 接入：生成分数、映射信号、写共享内存、A/B 对比
bt5_demo/            bt5 示例工程（已改：basket 格式、shm 信号）
bt5_work/, gh_stage/ bt5 头文件与运行库（.so 被忽略，见 §5）
overleaf_project/    【不进仓库】开题报告，独立 git 仓库（推 Overleaf）
results/             小体积结果（IC 表、搜索日志）
docs/                DEVELOPMENT.md / README.md / 本文件
AGENTS.md            环境坑与项目约定的速查（**接手先读这个**）
```

### 没有（需在新机器上准备）

| 数据 | 位置 | 规模 | 说明 |
|---|---|---|---|
| TickPool 原始逐笔 | `/opt/bar_data/TickPool/*.dat` | 23.9 GB/天 × 445 天 | 交易所 L2 全量，**不可重建** |
| 日频 DailyBar | `/opt/bar_data/DailyBar` | 1211 天 | 复权、市值、波动、行业等 |
| 股票元数据 | `/opt/bar_data/Meta/tkr.txt` | 5213 行 | cidx 顺序必须与此一致 |
| 分钟面板 | `pipeline/panels/*.panel` | 60 MB/天 × 70 天 | **可由 TickPool 重建**，约 50s～900s/天 |
| 盘口面板 | `pipeline/panel_books/*.panelbook` | 115 MB/天 × 12 天 | 可由 TickPool 重建 |
| 订单流入库 | ClickHouse `ofz` 库 | ~1 TiB | 由 `ingest/` 脚本从 tick 聚合 |

> **重建优先级**：面板可以直接重建（几小时/60 天），但 **TickPool 本身必须先拷贝过去**，
> 没有原始 tick 一切归零。拷贝 445 天约 8.5 TB。

---

## 3. 环境要求（这是最容易踩坑的部分）

实测环境：**CentOS 7（glibc 2.17）、80 核、251 GB 内存、`/opt` 单盘（约 240 MB/s）**。

### 3.1 Node 与 DSH（如果新账号也要跑 AI 助手）

坑：DSH 要求 Node ≥ 24.9，而 Node 24 官方二进制要求 **glibc ≥ 2.28**，CentOS 7 只有 2.17。

**错误做法**（本项目曾踩过，导致所有 shell 命令退码 127）：

```sh
export LD_LIBRARY_PATH=/path/to/glibc228/usr/lib64:...
```

`LD_LIBRARY_PATH` 会被**所有子进程继承**，`bash`/`git`/`make` 拿到「2.17 的加载器 + 2.28 的 libc」，
在 `main()` 之前就崩：`relocation error ... _dl_audit_symbind_alt ... GLIBC_PRIVATE`。

**正确做法**：用 `ld.so` 显式加载，环境变量绝不外泄：

```sh
#!/bin/sh
# /path/to/node/bin/node  —— 包装脚本
exec /path/to/glibc228/usr/lib64/ld-2.28.so \
     --library-path /path/to/glibc228/usr/lib64:/path/to/glibc228/lib64 \
     /path/to/node/bin/node-real "$@"
```

完整排错与回滚见 `AGENTS.md` §2 和 `FIX-NODE-WRAPPER.md`。

### 3.2 编译工具链

```sh
g++ --version      # 需支持 C++17（本项目实测 4.8.5 可编 C++11 部分，C++17 建议 devtoolset）
make
python3            # numpy / pandas
```

### 3.3 LaTeX（只写论文时需要）

本项目**宿主机未装** TeX Live，PDF 在 Overleaf 上编译。若需本地编译：
`xelatex` + `biber` + `xeCJK` + Fandol 字体（见 `overleaf_project/config.tex`）。

---

## 4. 恢复步骤（按顺序）

### 4.1 拷数据

```sh
# 必须：原始 tick（不可重建）
rsync -av /opt/bar_data/TickPool/ 新机器:/opt/bar_data/TickPool/

# 必须：股票元数据（cidx 顺序的基准）
rsync -av /opt/bar_data/Meta/     新机器:/opt/bar_data/Meta/

# 建议：日频（重建代价高）
rsync -av /opt/bar_data/DailyBar/ 新机器:/opt/bar_data/DailyBar/
```

### 4.2 编译

```sh
cd pipeline && ./build.sh          # 产出 bin/panel_tool alpha_expr backtest sim_live ...
```

### 4.3 重建面板

```sh
# 单日
./bin/panel_tool 20260105 panels

# 批量（脚本内按 5 路并行，实测 10 天 239 GB 约 1228 s）
for d in 20260105 20260106 20260107; do ./bin/panel_tool $d panels; done
```

### 4.4 跑通冒烟（验证迁移成功）

```sh
# 1) 因子
head -5 pipeline/template_factors.txt > /tmp/f5.txt
./bin/alpha_expr panels /tmp/f5.txt /tmp/f5.csv 20260105 20260106
#    期望：evaluated 5 factors in ~0.1s，输出含 IC1_mean/IR1/coverage

# 2) 分钟回测
./bin/backtest panels /tmp/f5.txt /tmp/bt 20 5 5 3 5 20260105 20260106
#    期望：backtested 5 factors in ~0.2s，summary 含 avg_net_bp/sharpe/cap_median

# 3) 逐 tick 模拟（约 2 分钟/天）
./bin/sim_live 20260105 /tmp/sim 20 5 0 0
#    期望：indexed ~2.0e8 ticks, trades 数千, 输出 equity/fills CSV
```

**基线数字（2026-11-08 实测，intern2 / 20260105）**：

| 检查项 | 期望值 |
|---|---|
| 单日 tick 总数 | 436,852,698（4.37 亿） |
| 面板 MD/TS/OD | 2,147 万 / 1.864 亿 / 2.288 亿 |
| 5 因子 ×2 天 alpha_expr | ≈ 0.1 s |
| 5 因子 ×2 天 backtest | ≈ 0.2 s，净收益约 **-18 bp**（印证 1 分钟不可交易） |
| sim_live 单日 | ≈ 1 分 44 秒，索引 2.05 亿条，成交约 4000 笔 |
| 面板单日 | 60 MB，240 × 5213 行 |

若这些数字对得上，说明数据与代码都迁移正确。

---

## 5. bt5 闭源撮合引擎的接入

### 5.1 资产位置

```
/opt/datas/lf_basket_trade/
  include/           头文件（lf_quant_backtrace_base.h、kernel/ 等）
  lib/               liblf_bt5.so、liblf_utillq.so、liblquant_kernel.so + hiredis/cryptopp
  t0_base_RF/        官方示例工程（我们改过，副本见 bt5_demo/）
```

`.so` 不进仓库（`.gitignore` 忽略），迁移时必须从原机拷贝。

### 5.2 三个必须知道的格式（踩过的坑）

| 项 | 正确形式 | 错误形式会怎样 |
|---|---|---|
| `--basketFile` | `TKR,QTY` 表头 + `000001.SZ,22500` 每行 | 只给代码列表 → 每行 `error line` |
| `--scoreFile` | **共享内存名**（如 `/bt5sig`），不是 CSV 路径 | 给 CSV → `failed to open shm` |
| 信号 `didx/tidx` | **都是全局 tick 下标**（TickPool 的 `get_data(k)` 序号） | 造假值 → 策略内部崩溃 |

### 5.3 编译示例工程（官方 makefile 不完整）

官方 `makefile` 缺三个依赖，直接 `make` 会链接失败：

```sh
g++ *.o -o demo -std=c++11 \
  -I /opt/datas/lf_basket_trade/include \
  -I /opt/datas/lf_basket_trade/include/kernel \
  -I /opt/datas/lf_basket_trade/include/include \
  -L/opt/datas/lf_basket_trade/lib -llf_utillq -llf_bt5 -llquant_kernel \
  /opt/datas/lf_basket_trade/lib/libhiredis_cluster.so.0.9 \
  /opt/datas/lf_basket_trade/lib/libhiredis.so.1.1.0 \
  -lpthread -lrt -ldl -Wl,-rpath,/opt/datas/lf_basket_trade/lib
```

还需要自己提供 `LQUANTSELFLog::logger` 的桩（库只引用不定义）：

```cpp
std::shared_ptr<lquantlog::logger> LQUANTSELFLog::logger;
```

### 5.4 完整跑一天的流程

```sh
cd bt5_ab
# ① 从面板生成分数：symbol,HHMMSSmmm,score
python3 gen_scores.py /home/intern2/rachel/pipeline/panels/20260105.panel scores_20260105.csv ../bt5_demo/basket.csv
# ② 映射到全局 tick 下标：didx,tidx,cidx,score
./make_signals 20260105 scores_20260105.csv signals_20260105.csv ../bt5_demo/basket.csv
# ③ 写入共享内存
./make_signal_shm /bt5sig 1000000 signals_20260105.csv
# ④ 跑引擎
cd ../bt5_demo
./demo --tickPath=/opt/bar_data --scoreFile=/bt5sig \
       --basketFile=$(pwd)/basket.csv --tradeDate=20260105
```

### 5.5 历史结果（2026-11-06，20260105 单日）

```
volume: 8,293,312 / 8,458,300 (98.05%)
amount: 101,325,679 / 104,378,079 (97.08%)
profit: 3,646.29 (0.36 BP)
```

产出：`bt5_ab/bt5_orders_20260105.csv`（3257 笔）、`bt5_events_20260105.csv`（13027 条）、
自研 `sim_replay*` 用于 A/B 对比，脚本 `compare_ab.py` / `compare_ab2.py`。

---

## 6. 已知问题（接手后大概率先撞上这些）

| 问题 | 现象 | 现状 |
|---|---|---|
| `sim_live` 的 `bad_ts` | 单日报 **32,200,146** 条时间戳异常 | 未定位；疑与集合竞价段或个别股票字段有关 |
| TickPool 非时间有序 | 文件内 tick 不按时间排列（2000 万条中 660 万逆序） | `sim_live` 用「按分钟分桶 + 桶内排序」解决 |
| `vwap` 与 OHLC 口径不同 | `vwap=amount/volume`（成交价），OHLC 是中间价 | 所有 `close/vwap` 类因子测的是价差位置，不是量价失衡 |
| 面板缺盘口字段 | 分钟面板只有 7 个价量字段 | 需改 `panel_tool` 加 OFI/spread/深度（见 `results/search/NOTES.md` Round 10） |
| 样本短 | 分钟面板仅 70 天 | 日频有 1211 天，alpha 发现建议放日频 |
| `liblf_utillq.so` 未定义符号 | 链接报 `LQUANTSELFLog::logger` | 自行提供桩，见 §5.3 |
| 宿主机无 TeX | `xelatex` 不在 PATH | 论文在 Overleaf 编译 |

---

## 7. 接手清单（checklist）

- [ ] 读 `AGENTS.md`（环境坑 + 仓库约定 + 提交要求）
- [ ] 拷 `/opt/bar_data/{TickPool,Meta,DailyBar}` 与 `/opt/datas/lf_basket_trade/lib`
- [ ] `pipeline/build.sh` 编译通过
- [ ] `panel_tool` 单日重建，与旧面板逐值对账（工具：`pipeline/tools/reconcile.py`）
- [ ] 跑 §4.4 三条冒烟，数字对上基线表
- [ ] 需要 bt5 时按 §5 走通，与 `sim_replay` 做 A/B
- [ ] 开题报告：`overleaf_project/` 是独立仓库，单独 `git pull`（远端在 `.git/config`，token 已内嵌）
- [ ] **不要在 `.bashrc` / `/etc/profile.d` 里 export `LD_LIBRARY_PATH`**

---

## 8. 交接备注

- 数据全部在 `/opt`，仓库只存代码与文档；**`git clone` 后不能直接跑**，必须按 §4 恢复数据。
- `pipeline/panels/`、`*.dat`、`*.so`、`pipeline/bin/`、`ofz/*.csv`、`*.panelbook` 都在 `.gitignore` 里。
- `overleaf.txt` 含 Overleaf git token，**已被忽略**，不要提交、不要打印。
- 历史基线结论（1 分钟反转扣 19 bp 不可交易、容量中位 15 万元、3 分钟暴跌事件 IC 0.09–0.17）见
  `results/search/NOTES.md` 与 `research_notes_20261031.md`。
