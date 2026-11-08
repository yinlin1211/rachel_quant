// L3 重建校验：用逐笔委托(OD)+成交(TS)重放一分钟，看能否还原真实十档。
//
// 方法：
//   1. 取 startMinute 结束时的 MD 十档作为初始订单簿
//   2. 按时间顺序重放 (startMinute, endMinute] 内的 OD 挂单/撤单 与 TS 成交
//   3. 与 endMinute 结束时的真实 MD 十档逐档比对
//
// 判定：价格与挂单量同时一致，才算该档匹配。
//
// 用法：validate_l3 <YYYYMMDD> <startMinute HHMM> <endMinute HHMM> [maxStocks]
//   例：validate_l3 20260105 935 940
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <unordered_map>

struct TickResult { int cidx; int type; void* p; };
extern "C" {
int init_tick_file(char* path, int date);
int get_tick_count();
TickResult get_tick(int idx);
}

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
struct QuantTdfOrder {
    int32_t nTime, nIndex, nPrice, nVolume;
    char nType, chBSFlag, chResv[2];
};
#pragma pack(pop)

static inline int ms_of_day(int nTime) {
    int t = nTime / 1000;
    int h = t / 10000, m = t / 100 % 100, s = t % 100;
    return ((h * 60 + m) * 60 + s) * 1000 + (nTime % 1000);
}
static inline int hhmm_to_min(int hhmm) { return (hhmm / 100) * 60 + (hhmm % 100); }

struct OrdState { uint32_t price; long long remaining; int8_t side; };
struct Snap {
    bool valid = false;
    uint32_t bp[10], bv[10], ap[10], av[10];
    int ms = 0;
};

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <YYYYMMDD> <startHHMM> <endHHMM> [addCode] [cancelCode]\n", argv[0]);
        return 1;
    }
    int date = atoi(argv[1]);
    int m0 = hhmm_to_min(atoi(argv[2]));
    int m1 = hhmm_to_min(atoi(argv[3]));
    std::string addCodes = (argc > 4) ? argv[4] : "0A";
    char cancelCode = (argc > 5) ? argv[5][0] : 'D';
    if (m1 <= m0) { fprintf(stderr, "end must be after start\n"); return 1; }

    char pbuf[] = "/opt/bar_data";
    init_tick_file(pbuf, date);
    int nTicks = get_tick_count();

    std::vector<int> tkr;
    {
        std::string f = "/opt/bar_data/Meta/tkr.txt";
        FILE* fp = fopen(f.c_str(), "r");
        if (!fp) { fprintf(stderr, "no tkr list\n"); return 1; }
        char line[128];
        while (fgets(line, sizeof(line), fp)) tkr.push_back(atoi(line));
        fclose(fp);
    }
    int N = (int)tkr.size();

    // 事件：只收集 [m0, m1] 区间，按毫秒排序
    struct Ev { int ms; int type; int k; };
    std::vector<Ev> evs;
    std::vector<Snap> startSnap(N), endSnap(N);
    long long scanned = 0;

    for (int k = 0; k < nTicks; ++k) {
        TickResult t = get_tick(k);
        int c = t.cidx;
        if (c < 0 || c >= N) continue;
        int nt;
        if (t.type == 0) nt = ((const QuantMarketData*)t.p)->nTime;
        else if (t.type == 1) nt = ((const QuantTransaction*)t.p)->nTime;
        else if (t.type == 2) nt = ((const QuantTdfOrder*)t.p)->nTime;
        else continue;
        int mn = ms_of_day(nt) / 60000;
        if (mn < m0 - 1 || mn > m1 + 1) continue;
        ++scanned;
        if (t.type == 0) {
            const QuantMarketData* md = (const QuantMarketData*)t.p;
            if (md->nBidPrice[0] <= 0 && md->nAskPrice[0] <= 0) continue;
            Snap s;
            s.valid = true; s.ms = ms_of_day(nt);
            for (int i = 0; i < 10; ++i) {
                s.bp[i] = md->nBidPrice[i]; s.bv[i] = md->nBidVol[i];
                s.ap[i] = md->nAskPrice[i]; s.av[i] = md->nAskVol[i];
            }
            if (mn == m0) { if (!startSnap[c].valid || s.ms > startSnap[c].ms) startSnap[c] = s; }
            if (mn == m1) { if (!endSnap[c].valid || s.ms > endSnap[c].ms) endSnap[c] = s; }
        } else {
            int mms = ms_of_day(nt);
            if (mn >= m0 && mn <= m1) {
                Ev e; e.ms = mms; e.type = t.type; e.k = k;
                evs.push_back(e);
            }
        }
    }
    std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.ms < b.ms; });
    fprintf(stderr, "scanned=%lld events=%zu\n", scanned, evs.size());

    // 每只股票的订单簿（价格 -> 挂单量），由订单表维护
    struct Book { std::map<uint32_t, long long> bid, ask; };
    std::vector<Book> books(N);
    std::vector<char> init(N, 0);
    for (int c = 0; c < N; ++c) {
        if (!startSnap[c].valid || !endSnap[c].valid) continue;
        Book& b = books[c];
        for (int i = 0; i < 10; ++i) {
            if (startSnap[c].bp[i] > 0 && startSnap[c].bv[i] > 0) b.bid[startSnap[c].bp[i]] += startSnap[c].bv[i];
            if (startSnap[c].ap[i] > 0 && startSnap[c].av[i] > 0) b.ask[startSnap[c].ap[i]] += startSnap[c].av[i];
        }
        init[c] = 1;
    }

    // 订单表：key = cidx<<32 | orderId；只用于本窗口内新增的单
    std::unordered_map<unsigned long long, OrdState> omap;
    omap.reserve(1 << 24);

    auto apply_vol = [&](int c, int8_t side, uint32_t px, long long dv) {
        if (px == 0 || dv == 0) return;
        std::map<uint32_t, long long>& m = (side > 0) ? books[c].bid : books[c].ask;
        m[px] += dv;
        if (m[px] <= 0) m.erase(px);
    };
    auto add_order = [&](int c, unsigned long long key, int8_t side, uint32_t px, long long vol) {
        if (vol <= 0 || px == 0) return;
        OrdState& o = omap[key];
        o.price = px; o.side = side; o.remaining = vol;
        apply_vol(c, side, px, vol);
    };
    auto kill_order = [&](int c, unsigned long long key, long long vol, bool whole) {
        std::unordered_map<unsigned long long, OrdState>::iterator it = omap.find(key);
        if (it == omap.end()) return;
        OrdState& o = it->second;
        long long d = whole ? o.remaining : std::min(vol, o.remaining);
        if (d <= 0) return;
        o.remaining -= d;
        apply_vol(c, o.side, o.price, -d);
        if (o.remaining <= 0) omap.erase(it);
    };

    long long nAdd = 0, nCancel = 0, nTrade = 0, missAdd = 0, missCancel = 0;

    for (size_t i = 0; i < evs.size(); ++i) {
        TickResult t = get_tick(evs[i].k);
        int c = t.cidx;
        if (c < 0 || c >= N || !init[c]) continue;
        if (evs[i].ms <= startSnap[c].ms || evs[i].ms > endSnap[c].ms) continue;

        if (t.type == 2) {
            const QuantTdfOrder* od = (const QuantTdfOrder*)t.p;
            unsigned long long key = ((unsigned long long)c << 32) | (unsigned int)od->nIndex;
            if (addCodes.find(od->nType) != std::string::npos) {
                add_order(c, key, (od->chBSFlag == 'B') ? 1 : -1, od->nPrice, od->nVolume);
                ++nAdd;
            } else if (od->nType == cancelCode) {
                // 订单未知（窗口开始前挂的单）时，按价格直接扣减，避免队列漂移
                if (omap.find(key) == omap.end()) {
                    apply_vol(c, (od->chBSFlag == 'B') ? 1 : -1, od->nPrice, -(long long)od->nVolume);
                    ++missCancel;
                } else {
                    kill_order(c, key, od->nVolume, true);
                }
                ++nCancel;
            }
        } else if (t.type == 1) {
            const QuantTransaction* ts = (const QuantTransaction*)t.p;
            if (ts->chFunctionCode == 'C') {              // 撤单
                unsigned long long k = ts->nAskOrder > 0 ? (((unsigned long long)c << 32) | (unsigned)ts->nAskOrder)
                                                        : (((unsigned long long)c << 32) | (unsigned)ts->nBidOrder);
                size_t before = omap.size();
                kill_order(c, k, 0, true);
                if (omap.size() == before) ++missCancel;
                ++nCancel;
            } else {                                      // 成交：同时扣减买卖两个订单
                if (ts->nVolume <= 0) continue;
                unsigned long long ka = ((unsigned long long)c << 32) | (unsigned)ts->nAskOrder;
                unsigned long long kb = ((unsigned long long)c << 32) | (unsigned)ts->nBidOrder;
                bool fa = (ts->nAskOrder > 0) && omap.count(ka);
                bool fb = (ts->nBidOrder > 0) && omap.count(kb);
                if (fa) kill_order(c, ka, ts->nVolume, false);
                if (fb) kill_order(c, kb, ts->nVolume, false);
                // 未知订单：按主动方向对被动方价格直接扣减
                if (ts->chBSFlag == 'B') {                 // 买方主动 -> 吃卖盘
                    if (!fa) apply_vol(c, -1, ts->nPrice, -(long long)ts->nVolume);
                } else {                                    // 卖方主动 -> 吃买盘
                    if (!fb) apply_vol(c, 1, ts->nPrice, -(long long)ts->nVolume);
                }
                if (!fa && !fb) ++missAdd;
                ++nTrade;
            }
        }
    }

    // 比对
    long long stockTotal = 0, stockAllMatch = 0;
    long long lvlMatch[10] = {0}, lvlTotal[10] = {0};
    long long volDiffAbs = 0, volTotal = 0;
    for (int c = 0; c < N; ++c) {
        if (!init[c]) continue;
        Book& b = books[c];
        // 取最优 10 档
        std::vector<std::pair<uint32_t, long long> > bid, ask;
        for (std::map<uint32_t, long long>::reverse_iterator it = b.bid.rbegin(); it != b.bid.rend() && (int)bid.size() < 10; ++it)
            if (it->second > 0) bid.push_back(*it);
        for (std::map<uint32_t, long long>::iterator it = b.ask.begin(); it != b.ask.end() && (int)ask.size() < 10; ++it)
            if (it->second > 0) ask.push_back(*it);

        ++stockTotal;
        bool all = true;
        for (int i = 0; i < 10; ++i) {
            // 买盘
            ++lvlTotal[i];
            if (i < (int)bid.size() && endSnap[c].bp[i] == bid[i].first &&
                endSnap[c].bv[i] == (uint32_t)bid[i].second) ++lvlMatch[i];
            else all = false;
            // 卖盘（计入同一档统计）
            ++lvlTotal[i];
            if (i < (int)ask.size() && endSnap[c].ap[i] == ask[i].first &&
                endSnap[c].av[i] == (uint32_t)ask[i].second) ++lvlMatch[i];
            else all = false;
            if (i < (int)bid.size()) { volTotal += endSnap[c].bv[i]; volDiffAbs += std::abs((long long)endSnap[c].bv[i] - bid[i].second); }
            if (i < (int)ask.size()) { volTotal += endSnap[c].av[i]; volDiffAbs += std::abs((long long)endSnap[c].av[i] - ask[i].second); }
        }
        if (all) ++stockAllMatch;
    }

    printf("window %02d:%02d -> %02d:%02d\n", m0 / 60, m0 % 60, m1 / 60, m1 % 60);
    printf("events: add=%lld cancel=%lld trade=%lld  (missing order refs: trade=%lld cancel=%lld)\n", nAdd, nCancel, nTrade, missAdd, missCancel);
    printf("stocks=%lld  all-10-levels-exact=%lld (%.2f%%)\n",
           stockTotal, stockAllMatch, stockTotal ? 100.0 * stockAllMatch / stockTotal : 0.0);
    printf("per-level exact match:\n");
    for (int i = 0; i < 10; ++i)
        printf("  L%-2d %6.2f%%\n", i + 1, lvlTotal[i] ? 100.0 * lvlMatch[i] / lvlTotal[i] : 0.0);
    printf("volume abs error ratio: %.4f%%\n", volTotal ? 100.0 * volDiffAbs / volTotal : 0.0);
    return 0;
}
