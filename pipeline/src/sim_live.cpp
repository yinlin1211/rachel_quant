// 逐 tick 回放式「模拟实盘」。
//
// 八个组件：
//   1 回放引擎    : 按毫秒顺序喂 MD/TS 事件，分钟边界触发策略
//   2 订单生命周期 : 新建 -> 激活(延迟) -> 部分成交 -> 完成 / 过期
//   3 撮合模型    : 市价单吃对手价（逐档扫），限价单只在穿价时成交，参与率上限
//   4 账户持仓    : 现金、持仓、可卖数量（T+1，隔夜结算）、成本
//   5 费用        : 佣金(2.5bp，最低 5 元)、印花税(卖出 5bp)、过户费(0.1bp)
//   6 流式因子    : 每只股票维护 20 根分钟 bar 的环形缓冲，盘中实时算因子
//   7 风控        : 涨停不买、跌停不卖、单票上限、参与率上限、无成交跳过
//   8 成交日志    : 逐笔 fills.csv（时间/股票/方向/量/价/费用/原因）
//
// 策略：暴跌反弹（与 research 中验证过的版本一致）
//   条件：成交额 20 分钟均值排前 50%，且 3 分钟累计收益 < -2σ（σ 用 20 分钟收益标准差）
//   打分：-(本分钟 low / 过去 5 分钟 low 均值 - 1)，取分数最高的 N 只
//   持有：H 分钟
//
// 用法：
//   sim_live <date> <out_prefix> [topN] [hold] [latency_ms] [max_ticks]
// 例：
//   sim_live 20260105 /tmp/sim 20 10 50 0
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <fstream>
#include <sstream>

struct TickResult { int cidx; int type; void* p; };
extern "C" {
int init_tick_file(char* path, int date);
int get_tick_count();
TickResult get_tick(int idx);
}

// 价格单位：0.0001 元（54900 = 5.49 元），最小变动价位 = 100（0.01 元）
static const double PX_SCALE = 10000.0;

#pragma pack(push, 1)
struct QuantMarketData {
    int32_t gid; int32_t nTime; int32_t nStatus;
    uint32_t nPreClose, nOpen, nHigh, nLow, nMatch;
    uint32_t nAskPrice[10], nAskVol[10], nBidPrice[10], nBidVol[10];
    uint32_t nNumTrades;
    int64_t iVolume, iTurnover, nTotalBidVol, nTotalAskVol;
    uint32_t nWeightedAvgBidPrice, nWeightedAvgAskPrice;
    int32_t nIOPV, nYieldToMaturity;
    uint32_t nHighLimited, nLowLimited;
    char chPrefix[4];
};
struct QuantTransaction {
    int32_t nTime, nIndex, nPrice, nVolume, nTurnover;
    char chBSFlag, chOrderKind, chFunctionCode, chResv;
    int32_t nAskOrder, nBidOrder;
};
#pragma pack(pop)

static_assert(sizeof(QuantMarketData) == 256, "MD size");
static_assert(sizeof(QuantTransaction) == 32, "TS size");

// ---------------------------------------------------------------- 基础类型
static inline int ms_of_day(int nTime) {          // HHMMSSmmm -> 当日毫秒
    int t = nTime / 1000;                          // HHMMSS
    int h = t / 10000, m = t / 100 % 100, s = t % 100;
    return ((h * 60 + m) * 60 + s) * 1000 + (nTime % 1000);
}
static inline int minute_of(int nTime) { return ms_of_day(nTime) / 60000; }
static inline bool in_session(int minute) {
    int hh = minute / 60, mm = minute % 60;
    int m = hh * 60 + mm;
    return (m >= 570 && m < 690) || (m >= 780 && m < 900);
}

struct Book {
    uint32_t bidP[10], askP[10], bidV[10], askV[10];
    uint32_t last, limitUp, limitDown, preClose;
    int lastMs;
    bool valid;
    Book() : last(0), limitUp(0), limitDown(0), preClose(0), lastMs(0), valid(false) {
        for (int i = 0; i < 10; ++i) { bidP[i] = askP[i] = bidV[i] = askV[i] = 0; }
    }
};

struct Bar {                       // 一分钟聚合
    float open, high, low, close;
    double vol, amt;
    bool has;
    Bar() : open(0), high(0), low(0), close(0), vol(0), amt(0), has(false) {}
};

// ---------------------------------------------------------------- 订单
enum Side { BUY = 1, SELL = -1 };
enum OrderStatus { NEW = 0, ACTIVE = 1, PARTIAL = 2, FILLED = 3, CANCELLED = 4, EXPIRED = 5 };

struct Order {
    long long id;
    int cidx;
    int side;
    long long qty;          // 剩余未成交
    long long qty0;
    uint32_t limitPx;       // 0 = 市价
    int submitMs, activateMs, expireMs;
    int status;
    int lastMinute;              // 参与率统计所在分钟
    long long filledThisMinute;  // 该分钟已成交数量
    double filledValue, fees;
    long long filledQty;
};

// ---------------------------------------------------------------- 账户
struct Account {
    double cash, initCash;
    std::vector<long long> pos, sellable;
    std::vector<double> cost;      // 持仓成本（用于统计）
    double feeTotal, stampTotal, commTotal;
    long long trades;
    Account(int n) : cash(0), initCash(0), pos(n, 0), sellable(n, 0), cost(n, 0.0),
                     feeTotal(0), stampTotal(0), commTotal(0), trades(0) {}
};

// ---------------------------------------------------------------- 参数
struct Cfg {
    int topN = 20;
    int holdMin = 10;
    int latencyMs = 50;
    int maxTicks = 0;          // 0 = 全部
    double participation = 0.01;
    double maxPosPct = 0.05;   // 单票占总权益上限
    double commBp = 2.5;
    double commMin = 5.0;
    double stampBp = 5.0;
    double transferBp = 0.1;
    bool t0 = true;            // 底仓 T+0：买入后即可卖出（不额外占用底仓资金）
};

// ---------------------------------------------------------------- 主类
struct Engine {
    Cfg cfg;
    int N;
    std::vector<Book> book;
    std::vector<Bar> bar;
    std::vector<std::vector<Bar> > hist;   // 每只股票近 20 根
    std::vector<Order> orders;
    Account acct;
    long long nextOrderId;
    int curMin;
    int minuteIdx;              // 已过分钟数，用于决定是否到换仓时点
    std::vector<int> holding;              // 入场分钟，-1 未持有
    std::vector<int> activeOrder;          // 每只股票当前未了结的订单下标（-1 表示无）
    FILE* fillLog;
    FILE* equityLog;
    long long blockedLimitUp, blockedLimitDown, blockedNoSellable, blockedRisk, filledOrders, badTs;

    Engine(int n) : N(n), book(n), bar(n), hist(n, std::vector<Bar>(20)), acct(n),
                    nextOrderId(1), curMin(-1), minuteIdx(0), holding(n, -1), activeOrder(n, -1), fillLog(0), equityLog(0),
                    blockedLimitUp(0), blockedLimitDown(0), blockedNoSellable(0),
                    blockedRisk(0), filledOrders(0), badTs(0) {}

    double equity() const {                        // 以最新价估值
        double v = acct.cash;
        for (int c = 0; c < N; ++c) {
            if (acct.pos[c] == 0) continue;
            double px = book[c].valid ? (double)book[c].last / PX_SCALE : acct.cost[c];
            v += acct.pos[c] * px;
        }
        return v;
    }

    void pushHistory(int c) {                     // 分钟换根
        Bar& b = bar[c];
        if (!b.has) return;
        std::vector<Bar>& h = hist[c];
        for (int i = 0; i < 19; ++i) h[i] = h[i + 1];
        h[19] = b;
        b = Bar();
    }

    // ---- 组件 7：风控 + 组件 3：撮合 ----
    bool canTrade(int c, int side) const {
        const Book& bk = book[c];
        if (!bk.valid) return false;
        if (side == BUY && bk.limitUp > 0 && bk.last >= bk.limitUp) return false;   // 涨停不买
        if (side == SELL && bk.limitDown > 0 && bk.last <= bk.limitDown) return false; // 跌停不卖
        if (side == SELL && acct.sellable[c] <= 0) return false;                     // T+1
        return true;
    }

    // 撮合：市价单吃对手价逐档扫；限价单需穿价；受参与率限制
    void tryFill(Order& o, int nowMs, double maxQtyByVolume) {
        Book& bk = book[o.cidx];
        if (!bk.valid) return;
        if (nowMs < o.activateMs) return;
        if (nowMs > o.expireMs) { o.status = EXPIRED; activeOrder[o.cidx] = -1; return; }
        if (!canTrade(o.cidx, o.side)) {
            if (o.side == BUY) { if (bk.limitUp > 0 && bk.last >= bk.limitUp) ++blockedLimitUp; }
            else {
                if (bk.limitDown > 0 && bk.last <= bk.limitDown) ++blockedLimitDown;
                else if (acct.sellable[o.cidx] <= 0) ++blockedNoSellable;
            }
            o.status = CANCELLED;
            activeOrder[o.cidx] = -1;
            return;
        }
        if (o.qty <= 0) return;

        int curMinute = nowMs / 60000;
        if (curMinute != o.lastMinute) { o.lastMinute = curMinute; o.filledThisMinute = 0; }
        long long want = o.qty;
        if (maxQtyByVolume > 0) {
            long long remainCap = (long long)maxQtyByVolume - o.filledThisMinute;
            if (remainCap <= 0) return;              // 本分钟参与率已用满
            if (want > remainCap) want = remainCap;
        }
        if (want <= 0) return;

        double got = 0;
        long long gotQty = 0;
        if (o.side == BUY) {
            for (int lvl = 0; lvl < 10 && gotQty < want; ++lvl) {
                uint32_t px = bk.askP[lvl], av = bk.askV[lvl];
                if (px == 0 || av == 0) continue;
                if (o.limitPx > 0 && px > o.limitPx) break;      // 限价买：只能吃到 <= 限价
                long long take = std::min<long long>(av, want - gotQty);
                got += take * (double)px;
                gotQty += take;
            }
        } else {
            for (int lvl = 0; lvl < 10 && gotQty < want; ++lvl) {
                uint32_t px = bk.bidP[lvl], bv = bk.bidV[lvl];
                if (px == 0 || bv == 0) continue;
                if (o.limitPx > 0 && px < o.limitPx) break;      // 限价卖：只能吃 >= 限价
                long long take = std::min<long long>(bv, want - gotQty);
                got += take * (double)px;
                gotQty += take;
            }
        }
        if (gotQty <= 0) return;
        double avgPx = got / gotQty;                 // 0.0001 元
        double value = avgPx * gotQty / PX_SCALE;    // 元
        // 佣金按订单累计，最低 5 元只收一次（在订单了结时结算）
        double transfer = value * cfg.transferBp / 1e4;
        double stamp = (o.side == SELL) ? value * cfg.stampBp / 1e4 : 0.0;
        double fee = transfer + stamp;
        acct.commTotal += transfer;
        acct.stampTotal += stamp;
        acct.feeTotal += fee;

        if (o.side == BUY) {
            acct.cash -= (value + fee);
            acct.pos[o.cidx] += gotQty;
            if (cfg.t0) acct.sellable[o.cidx] += gotQty;   // 底仓 T+0 假设
            acct.cost[o.cidx] = (acct.cost[o.cidx] * (acct.pos[o.cidx] - gotQty) + value) /
                                std::max<long long>(1, acct.pos[o.cidx]);
        } else {
            acct.cash += (value - fee);
            acct.pos[o.cidx] -= gotQty;
            acct.sellable[o.cidx] -= gotQty;
        }
        acct.trades++;
        o.qty -= gotQty;
        o.filledQty += gotQty;
        o.filledThisMinute += gotQty;
        o.filledValue += value;
        o.fees += fee;
        o.status = (o.qty == 0) ? FILLED : PARTIAL;
        if (o.status == FILLED) activeOrder[o.cidx] = -1;
        if (o.status == FILLED) {
            ++filledOrders;
            double comm = std::max(cfg.commMin, o.filledValue * cfg.commBp / 1e4);
            acct.cash -= (o.side == BUY ? comm : -comm);
            acct.commTotal += comm;
            acct.feeTotal += comm;
            o.fees += comm;
        }

        fprintf(fillLog, "%s,%d,%d,%lld,%.4f,%.2f,%lld,%lld\n",
                (o.side == BUY ? "B" : "S"), o.cidx, nowMs, gotQty, avgPx / PX_SCALE, fee, o.qty, o.id);
    }

    void compactOrders() {
        std::vector<Order> keep;
        keep.reserve(orders.size());
        std::fill(activeOrder.begin(), activeOrder.end(), -1);
        for (size_t i = 0; i < orders.size(); ++i) {
            Order& o = orders[i];
            if (o.status == FILLED || o.status == CANCELLED || o.status == EXPIRED) continue;
            activeOrder[o.cidx] = (int)keep.size();
            keep.push_back(o);
        }
        orders.swap(keep);
    }

    void resetDaily() {
        for (int c = 0; c < N; ++c) {
            acct.sellable[c] = acct.pos[c];             // 隔夜结算，昨日持仓今日可卖
            holding[c] = -1;                            // 新交易日重新计持有期
        }
    }

    // ---- 组件 6：流式因子 + 策略信号 ----
    // 在分钟边界调用：hist[19] 是刚结束的这一分钟
    void onMinute() {
        ++minuteIdx;
        // 只在每个持有期的边界换仓：持仓正好 H 分钟后整体了结，再建新仓
        if ((minuteIdx % cfg.holdMin) != 0) return;

        struct Cand { float score; int c; };
        std::vector<Cand> cand;
        std::vector<std::pair<double, int> > amtRank;
        amtRank.reserve(N);

        // 1) 成交额 20 分钟均值，用于截面排名
        std::vector<double> amt20(N, -1.0);
        for (int c = 0; c < N; ++c) {
            const std::vector<Bar>& h = hist[c];
            if (!h[19].has) continue;
            double s = 0; int n = 0;
            for (int i = 0; i < 20; ++i) if (h[i].has) { s += h[i].amt; ++n; }
            if (n >= 10) { amt20[c] = s / n; amtRank.push_back(std::make_pair(amt20[c], c)); }
        }
        if (amtRank.empty()) return;
        std::sort(amtRank.begin(), amtRank.end());
        std::vector<double> amtPct(N, 0.0);
        for (size_t i = 0; i < amtRank.size(); ++i)
            amtPct[amtRank[i].second] = (double)i / (double)amtRank.size();

        // 2) 计算 z 与打分
        for (int c = 0; c < N; ++c) {
            const std::vector<Bar>& h = hist[c];
            if (!h[19].has || amt20[c] < 0) continue;
            if (amtPct[c] < 0.5) continue;                       // 成交额前 50%
            // 20 分钟收益标准差
            double sum = 0, sum2 = 0; int n = 0;
            for (int i = 19; i > 0; --i) {
                if (!h[i].has || !h[i - 1].has || h[i - 1].close <= 0) continue;
                double r = (double)h[i].close / h[i - 1].close - 1.0;
                sum += r; sum2 += r * r; ++n;
            }
            if (n < 10) continue;
            double mean = sum / n;
            double var = sum2 / n - mean * mean;
            double sd = var > 0 ? std::sqrt(var) : 0.0;
            if (sd <= 1e-9) continue;
            // 3 分钟累计收益
            double c0 = h[16].close, c1 = h[19].close;
            if (c0 <= 0 || !h[16].has) continue;
            double r3 = c1 / c0 - 1.0;
            double z = r3 / sd;
            if (z >= -2.0) continue;                             // 暴跌条件
            // 打分：本分钟 low 相对近 5 分钟 low 均值
            double ls = 0; int ln = 0;
            for (int i = 15; i <= 19; ++i) if (h[i].has) { ls += h[i].low; ++ln; }
            if (ln < 5) continue;
            double lm = ls / ln;
            if (lm <= 0) continue;
            float score = -(float)(h[19].low / lm - 1.0);
            cand.push_back(Cand{score, c});
        }
        if (cand.empty()) return;
        std::partial_sort(cand.begin(),
                          cand.begin() + std::min<int>(cfg.topN, cand.size()),
                          cand.end(),
                          [](const Cand& a, const Cand& b) { return a.score > b.score; });
        int k = std::min<int>(cfg.topN, cand.size());

        std::vector<char> want(N, 0);
        for (int i = 0; i < k; ++i) want[cand[i].c] = 1;

        // 3) 生成订单：先卖后买
        double eq = equity();
        double perName = eq / std::max(1, cfg.topN);
        for (int c = 0; c < N; ++c) {
            Book& bk = book[c];
            if (!bk.valid || bk.last == 0) continue;
            if (activeOrder[c] >= 0) continue;      // 已有在途订单，避免重复
            if (acct.pos[c] > 0) {
                Order o;
                o.id = nextOrderId++; o.cidx = c; o.side = SELL;
                o.qty = std::min<long long>(acct.pos[c], acct.sellable[c]);
                if (o.qty <= 0) { ++blockedNoSellable; continue; }
                o.qty0 = o.qty; o.limitPx = 0;
                o.submitMs = bk.lastMs;
                o.activateMs = bk.lastMs + cfg.latencyMs;
                o.expireMs = bk.lastMs + cfg.holdMin * 60000;
                o.status = NEW; o.filledValue = 0; o.fees = 0; o.filledQty = 0;
                o.lastMinute = -1; o.filledThisMinute = 0;
                activeOrder[c] = (int)orders.size();
                orders.push_back(o);
            } else if (want[c]) {
                if (acct.pos[c] > 0) { holding[c] = curMin; continue; }
                double cap = std::min(perName, acct.cash * 0.9);
                if (cap <= 0) continue;
                double pxYuan = (double)bk.last / PX_SCALE;
                long long qty = (long long)(cap / pxYuan / 100.0) * 100;
                if (qty <= 0) continue;
                Order o;
                o.id = nextOrderId++; o.cidx = c; o.side = BUY;
                o.qty = qty; o.qty0 = qty; o.limitPx = 0;
                o.submitMs = bk.lastMs;
                o.activateMs = bk.lastMs + cfg.latencyMs;
                o.expireMs = bk.lastMs + cfg.holdMin * 60000;
                o.status = NEW; o.filledValue = 0; o.fees = 0; o.filledQty = 0;
                o.lastMinute = -1; o.filledThisMinute = 0;
                activeOrder[c] = (int)orders.size();
                orders.push_back(o);
                holding[c] = curMin;
            }
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <YYYYMMDD> <out_prefix> [topN] [hold] [latency_ms] [max_ticks]\n", argv[0]);
        return 1;
    }
    int date = atoi(argv[1]);
    std::string prefix = argv[2];
    Cfg cfg;
    if (argc > 3) cfg.topN = atoi(argv[3]);
    if (argc > 4) cfg.holdMin = atoi(argv[4]);
    if (argc > 5) cfg.latencyMs = atoi(argv[5]);
    if (argc > 6) cfg.maxTicks = atoi(argv[6]);
    // E6 冲击成本实验：允许用环境变量覆盖参与率上限（不占用已有参数位）
    //   SIM_PARTICIPATION=0.005 ./bin/sim_live 20260105 out 20 5 0 0
    if (const char* p = getenv("SIM_PARTICIPATION")) {
        double v = atof(p);
        if (v > 0.0 && v <= 1.0) cfg.participation = v;
        else fprintf(stderr, "warn: SIM_PARTICIPATION=%s 无效，用默认 %.4f\n", p, cfg.participation);
    }
    // 打印生效参数，便于实验复现
    fprintf(stderr, "cfg: topN=%d holdMin=%d latencyMs=%d participation=%.4f\n",
            cfg.topN, cfg.holdMin, cfg.latencyMs, cfg.participation);

    std::string tickPath = "/opt/bar_data";
    std::vector<char> pb(tickPath.begin(), tickPath.end()); pb.push_back('\0');
    init_tick_file(&pb[0], date);
    int nTicks = get_tick_count();
    if (cfg.maxTicks > 0 && cfg.maxTicks < nTicks) nTicks = cfg.maxTicks;

    // 股票列表
    std::vector<int> tkrs;
    {
        std::string file = tickPath + "/Meta/tkr.txt";
        FILE* f = fopen(file.c_str(), "r");
        if (!f) { fprintf(stderr, "cannot open %s\n", file.c_str()); return 1; }
        char line[128];
        while (fgets(line, sizeof(line), f)) tkrs.push_back(atoi(line));
        fclose(f);
    }
    int N = (int)tkrs.size();

    Engine eng(N);
    eng.cfg = cfg;
    double initEq = 1000000.0;      // 初始资金，默认 100 万
    if (const char* e = getenv("SIM_EQUITY")) {          // E6 冲击成本实验可放大
        double v = atof(e);
        if (v >= 1e4) initEq = v;
        else fprintf(stderr, "warn: SIM_EQUITY=%s 过小，用默认\n", e);
    }
    eng.acct.cash = initEq;
    eng.acct.initCash = eng.acct.cash;
    eng.resetDaily();

    // ---- 可选：订单回放模式（用于和 bt5 做逐笔 A/B）----
    struct RepOrd { int cidx; int side; long long qty; int ms; uint32_t limitPx; std::string sym; };
    std::vector<RepOrd> repOrds;
    bool replayMode = (argc > 7);
    if (replayMode) {
        std::unordered_map<std::string, int> sym2c;
        {
            std::ifstream tf(tickPath + "/Meta/tkr.txt");
            std::string line; int cc = 0;
            while (std::getline(tf, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
                if (!line.empty()) sym2c[line] = cc;
                ++cc;
            }
        }
        std::ifstream of(argv[7]);
        if (!of) { fprintf(stderr, "cannot open orders file %s\n", argv[7]); return 1; }
        std::string line;
        while (std::getline(of, line)) {
            if (line.empty() || line[0] == '#') continue;
            char sym[64]; int tm = 0; char side = 0; long long qty = 0; unsigned int lp = 0;
            int nf = sscanf(line.c_str(), "%63[^,],%d,%c,%lld,%u", sym, &tm, &side, &qty, &lp);
            if (nf < 4 || (side != 'B' && side != 'S') || qty <= 0) continue;
            auto it = sym2c.find(sym);
            if (it == sym2c.end()) continue;
            RepOrd ro;
            ro.sym = sym;
            ro.cidx = it->second;
            ro.side = (side == 'B') ? BUY : SELL;
            ro.qty = qty;
            ro.limitPx = (nf >= 5) ? lp : 0;
            int hh = tm / 10000000, mi = (tm / 100000) % 100, ss = (tm / 1000) % 100, mss = tm % 1000;
            ro.ms = ((hh * 60 + mi) * 60 + ss) * 1000 + mss;
            repOrds.push_back(ro);
        }
        std::sort(repOrds.begin(), repOrds.end(),
                  [](const RepOrd& a, const RepOrd& b) { return a.ms < b.ms; });
        // 回放模式下不受资金/底仓约束，只比较撮合价格
        for (int i = 0; i < N; ++i) eng.acct.sellable[i] = 1000000000LL;
        eng.acct.cash = 1e12;
        fprintf(stderr, "replay orders=%zu from %s\n", repOrds.size(), argv[7]);
    }
    size_t repIdx = 0;

    char p1[1024], p2[1024];
    snprintf(p1, sizeof(p1), "%s.fills.csv", prefix.c_str());
    snprintf(p2, sizeof(p2), "%s.equity.csv", prefix.c_str());
    eng.fillLog = fopen(p1, "w");
    eng.equityLog = fopen(p2, "w");
    if (!eng.fillLog || !eng.equityLog) { fprintf(stderr, "cannot write logs\n"); return 1; }
    fprintf(eng.fillLog, "side,cidx,time_ms,qty,price,fee,remain,order_id\n");
    FILE* repLog = 0;
    if (replayMode) {
        std::string rp = prefix + ".replay.csv";
        repLog = fopen(rp.c_str(), "w");
        if (repLog) fprintf(repLog, "id,symbol,side,qty,ms,limit_px\n");
    }
    fprintf(eng.equityLog, "minute,equity,cash,positions,fills\n");

    // ---- 组件 1：两遍回放 ----
    // 实测：文件里的 tick 并非按时间排序（2000 万条中 660 万条逆序，最大回退 174 秒），
    // 所以不能顺序回放。第一遍按分钟分桶，第二遍逐分钟按毫秒排序后回放。
    struct Ev { int ms; int k; };
    std::vector<std::vector<Ev> > bucket(240);
    long long indexed = 0;
    for (int k = 0; k < nTicks; ++k) {
        TickResult t = get_tick(k);
        if (t.type != 0 && t.type != 1) continue;
        int nt = (t.type == 0) ? ((const QuantMarketData*)t.p)->nTime
                               : ((const QuantTransaction*)t.p)->nTime;
        int mn = minute_of(nt);
        int slot = -1;
        if (mn >= 570 && mn < 690) slot = mn - 570;
        else if (mn >= 780 && mn < 900) slot = 120 + (mn - 780);
        if (slot < 0) continue;
        Ev e; e.ms = ms_of_day(nt); e.k = k;
        bucket[slot].push_back(e);
        ++indexed;
    }
    fprintf(stderr, "indexed %lld market/trade ticks\n", indexed);

    int processed = 0;
    for (int slot = 0; slot < 240; ++slot) {
        if (bucket[slot].empty()) continue;
        std::sort(bucket[slot].begin(), bucket[slot].end(),
                  [](const Ev& a, const Ev& b) { return a.ms < b.ms; });
        int slotMinute = (slot < 120) ? (570 + slot) : (780 + slot - 120);

        for (size_t ei = 0; ei < bucket[slot].size(); ++ei) {
            const Ev& e = bucket[slot][ei];
            TickResult t = get_tick(e.k);
            int c = t.cidx;
            if (c < 0 || c >= N) continue;
            if (t.type == 0) {
                const QuantMarketData* md = (const QuantMarketData*)t.p;
                if (md->nBidPrice[0] <= 0 && md->nAskPrice[0] <= 0) continue;
                Book& bk = eng.book[c];
                for (int i = 0; i < 10; ++i) {
                    bk.bidP[i] = md->nBidPrice[i]; bk.bidV[i] = md->nBidVol[i];
                    bk.askP[i] = md->nAskPrice[i]; bk.askV[i] = md->nAskVol[i];
                }
                bk.limitUp = md->nHighLimited; bk.limitDown = md->nLowLimited;
                bk.preClose = md->nPreClose;
                bk.lastMs = e.ms;
                bk.valid = true;
                uint32_t mid = (md->nBidPrice[0] > 0 && md->nAskPrice[0] > 0)
                                   ? (md->nBidPrice[0] + md->nAskPrice[0]) / 2
                                   : md->nBidPrice[0] + md->nAskPrice[0];
                bk.last = mid;
                Bar& b = eng.bar[c];
                if (!b.has) { b.open = b.high = b.low = b.close = (float)mid; b.has = true; }
                else {
                    if ((float)mid > b.high) b.high = (float)mid;
                    if ((float)mid < b.low) b.low = (float)mid;
                    b.close = (float)mid;
                }
            } else {
                const QuantTransaction* ts = (const QuantTransaction*)t.p;
                if (ts->nPrice <= 0) { ++eng.badTs; continue; }
                Bar& b = eng.bar[c];
                b.vol += ts->nVolume; b.amt += ts->nTurnover;
                if (!b.has) { b.open = b.high = b.low = b.close = (float)ts->nPrice; b.has = true; }
            }

            // 回放模式：到达时间的订单在此刻提交
            if (replayMode) {
                while (repIdx < repOrds.size() && repOrds[repIdx].ms <= e.ms) {
                    const RepOrd& ro = repOrds[repIdx++];
                    int rc = ro.cidx;
                    if (rc < 0 || rc >= N) continue;
                    if (eng.activeOrder[rc] >= 0) continue;   // 该股票已有在途单
                    Order o;
                    o.id = eng.nextOrderId++;
                    o.cidx = rc;
                    o.side = ro.side;
                    o.qty = ro.qty;
                    o.qty0 = ro.qty;
                    o.limitPx = ro.limitPx;
                    o.submitMs = e.ms;
                    o.activateMs = e.ms + eng.cfg.latencyMs;
                    o.expireMs = e.ms + eng.cfg.holdMin * 60000;
                    o.status = NEW;
                    o.lastMinute = -1;
                    o.filledThisMinute = 0;
                    o.filledValue = 0;
                    o.fees = 0;
                    o.filledQty = 0;
                    eng.activeOrder[rc] = (int)eng.orders.size();
                    eng.orders.push_back(o);
                    if (repLog)
                        fprintf(repLog, "%lld,%s,%c,%lld,%d,%u\n",
                                o.id, ro.sym.c_str(), (ro.side == BUY ? 'B' : 'S'),
                                ro.qty, ro.ms, ro.limitPx);
                }
            }

            // 组件 2/3：只推进这只股票的在途订单
            int ai = eng.activeOrder[c];
            if (ai >= 0) {
                double capQty = 0;
                const std::vector<Bar>& h = eng.hist[c];
                if (h[19].has) capQty = h[19].vol * cfg.participation;
                eng.tryFill(eng.orders[ai], e.ms, capQty);
            }
            ++processed;
        }

        // 分钟收盘：换根、跑策略、记权益
        for (int i = 0; i < N; ++i) eng.pushHistory(i);
        if (!replayMode) eng.onMinute();
        int npos = 0;
        for (int i = 0; i < N; ++i) if (eng.acct.pos[i] != 0) ++npos;
        fprintf(eng.equityLog, "%d,%.2f,%.2f,%d,%lld\n", slotMinute, eng.equity(),
                eng.acct.cash, npos, eng.acct.trades);
        eng.curMin = slotMinute;
        eng.compactOrders();
        std::vector<Ev>().swap(bucket[slot]);   // 释放该分钟索引
    }

    // 收盘结算
    int npos = 0; long long shares = 0;
    for (int i = 0; i < N; ++i) if (eng.acct.pos[i] != 0) { ++npos; shares += eng.acct.pos[i]; }
    double finalEq = eng.equity();
    fclose(eng.fillLog); fclose(eng.equityLog);
    printf("date=%d ticks=%d stocks=%d minutes=%d\n", date, processed, N, eng.curMin);
    printf("equity_init=%.2f equity_final=%.2f ret=%.4f%%\n", eng.acct.initCash, finalEq,
           (finalEq / eng.acct.initCash - 1.0) * 100.0);
    printf("trades=%lld fees=%.2f (comm+transfer=%.2f stamp=%.2f)\n",
           eng.acct.trades, eng.acct.feeTotal, eng.acct.commTotal, eng.acct.stampTotal);
    printf("open_positions=%d shares=%lld cash=%.2f\n", npos, shares, eng.acct.cash);
    printf("blocked: limit_up=%lld limit_down=%lld no_sellable=%lld filled_orders=%lld bad_ts=%lld\n",
           eng.blockedLimitUp, eng.blockedLimitDown, eng.blockedNoSellable, eng.filledOrders, eng.badTs);
    return 0;
}
