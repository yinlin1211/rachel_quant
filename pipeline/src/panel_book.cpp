// 从 TickPool 生成「分钟 × 股票」面板，并在 OHLCV 之外补上盘口字段。
//
// 与 panel_tool 的区别：panel_tool 只保留中间价 OHLC + 成交量；本工具额外保留
//   bid1/ask1           分钟末买一卖一 -> 真实价差
//   bidvol1/askvol1     分钟末一档挂单量
//   totbid/totask       分钟末总委买/委卖量 -> 深度失衡
//   buyvol/sellvol      按 chBSFlag 拆分的主动买卖量
//   ofi                 分钟累计订单流失衡（Cont 定义，逐 tick 累加）
//   limitUp/limitDown   涨跌停价 -> 回测里禁止穿价成交
//
// 输出格式与 .panel 不兼容（行宽不同），单独放一个目录，避免影响现有流水线。
//
// 用法：panel_book <YYYYMMDD> <out_dir>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <cerrno>
#include <sys/stat.h>

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

struct PanelBookRow {
    int32_t nTime, minute, cidx, nMd, nTs;
    float open, high, low, close, vwap, volume, amount;
    float bid1, ask1, bidvol1, askvol1, totbid, totask;
    float buyvol, sellvol, ofi, limitUp, limitDown;
};
#pragma pack(pop)

static_assert(sizeof(QuantMarketData) == 256, "MD size");
static_assert(sizeof(QuantTransaction) == 32, "TS size");

static const int NSLOT = 240;

static inline int hhmmss_to_s(int x) { return x / 10000 * 3600 + (x / 100 % 100) * 60 + x % 100; }
static inline int s_to_hhmmss(int s) { return s / 3600 * 10000 + (s % 3600 / 60) * 100 + s % 60; }

static inline int minute_to_slot(int minute) {
    int hh = minute / 10000, mm = minute / 100 % 100;
    int m = hh * 60 + mm;
    if (m >= 570 && m < 690) return m - 570;
    if (m >= 780 && m < 900) return 120 + (m - 780);
    return -1;
}
static inline int slot_to_minute(int slot) {
    int m = (slot < 120) ? (570 + slot) : (780 + slot - 120);
    return m / 60 * 10000 + m % 60 * 100;
}

static std::vector<int> load_tkr_list(const std::string& path) {
    std::vector<int> v;
    std::string file = path + "/Meta/tkr.txt";
    FILE* f = fopen(file.c_str(), "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", file.c_str()); exit(1); }
    char line[128];
    while (fgets(line, sizeof(line), f)) v.push_back(atoi(line));
    fclose(f);
    return v;
}

static inline bool is_buy_flag(char c) { return c == 'B' || c == '1' || c == 'b'; }
static inline bool is_sell_flag(char c) { return c == 'S' || c == '2' || c == 's'; }

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <YYYYMMDD> <out_dir>\n", argv[0]); return 1; }
    int date = atoi(argv[1]);
    std::string outdir = argv[2];
    if (mkdir(outdir.c_str(), 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir %s failed: %s\n", outdir.c_str(), strerror(errno));
        return 1;
    }

    std::string tickPath = "/opt/bar_data";
    std::vector<char> pathBuf(tickPath.begin(), tickPath.end());
    pathBuf.push_back('\0');
    init_tick_file(&pathBuf[0], date);
    int nTicks = get_tick_count();

    std::vector<int> tkrs = load_tkr_list(tickPath);
    int N = (int)tkrs.size();

    const float NANV = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> op(N * NSLOT, NANV), hi(N * NSLOT, NANV), lo(N * NSLOT, NANV), cl(N * NSLOT, NANV);
    std::vector<double> vol(N * NSLOT, 0.0), amt(N * NSLOT, 0.0);
    std::vector<float> bid1(N * NSLOT, NANV), ask1(N * NSLOT, NANV);
    std::vector<float> bv1(N * NSLOT, 0.f), av1(N * NSLOT, 0.f);
    std::vector<float> tbid(N * NSLOT, 0.f), task(N * NSLOT, 0.f);
    std::vector<double> bvol(N * NSLOT, 0.0), svol(N * NSLOT, 0.0);
    std::vector<double> ofi(N * NSLOT, 0.0);
    std::vector<float> limU(N * NSLOT, NANV), limD(N * NSLOT, NANV);
    std::vector<int32_t> ntime(N * NSLOT, 0), nmd(N * NSLOT, 0), nts(N * NSLOT, 0);
    std::vector<unsigned char> has(N * NSLOT, 0);

    // OFI 需要「上一笔 MD」的状态，按股票保存
    std::vector<uint32_t> pBidP(N, 0), pAskP(N, 0), pBidV(N, 0), pAskV(N, 0);
    std::vector<unsigned char> pInit(N, 0);

    const int limit = nTicks - 500;
    for (int k = 0; k < limit; ++k) {
        TickResult t = get_tick(k);
        int c = t.cidx;
        if (c < 0 || c >= N) continue;
        if (t.type == 0) {
            const QuantMarketData* md = (const QuantMarketData*)t.p;
            if (md->nBidPrice[0] <= 0 && md->nAskPrice[0] <= 0) continue;
            int minute = s_to_hhmmss(hhmmss_to_s(md->nTime / 1000) / 60 * 60);
            int slot = minute_to_slot(minute);
            if (slot < 0) continue;
            double mid;
            if (md->nBidPrice[0] <= 0) mid = (double)md->nAskPrice[0];
            else if (md->nAskPrice[0] <= 0) mid = (double)md->nBidPrice[0];
            else mid = ((double)md->nAskPrice[0] + (double)md->nBidPrice[0]) / 2.0;
            size_t idx = (size_t)c * NSLOT + slot;
            if (!has[idx]) {
                op[idx] = hi[idx] = lo[idx] = cl[idx] = (float)mid;
                ntime[idx] = md->nTime;
                has[idx] = 1;
            } else {
                if ((float)mid > hi[idx]) hi[idx] = (float)mid;
                if ((float)mid < lo[idx]) lo[idx] = (float)mid;
                cl[idx] = (float)mid;
            }
            bid1[idx] = (float)md->nBidPrice[0];
            ask1[idx] = (float)md->nAskPrice[0];
            bv1[idx] = (float)md->nBidVol[0];
            av1[idx] = (float)md->nAskVol[0];
            tbid[idx] = (float)md->nTotalBidVol;
            task[idx] = (float)md->nTotalAskVol;
            if (md->nHighLimited > 0) limU[idx] = (float)md->nHighLimited;
            if (md->nLowLimited > 0) limD[idx] = (float)md->nLowLimited;
            nmd[idx]++;

            // Cont 订单流失衡：只比较相邻两笔 MD 的最优档
            if (pInit[c]) {
                double e = 0.0;
                uint32_t bp = md->nBidPrice[0], ap = md->nAskPrice[0];
                uint32_t bv = md->nBidVol[0], av = md->nAskVol[0];
                if (bp > pBidP[c]) e += bv;
                else if (bp < pBidP[c]) e -= pBidV[c];
                else e += (double)bv - (double)pBidV[c];
                if (ap < pAskP[c]) e += av;
                else if (ap > pAskP[c]) e -= pAskV[c];
                else e -= (double)av - (double)pAskV[c];
                ofi[idx] += e;
            }
            pBidP[c] = md->nBidPrice[0]; pAskP[c] = md->nAskPrice[0];
            pBidV[c] = md->nBidVol[0];   pAskV[c] = md->nAskVol[0];
            pInit[c] = 1;
        } else if (t.type == 1) {
            const QuantTransaction* ts = (const QuantTransaction*)t.p;
            int minute = s_to_hhmmss(hhmmss_to_s(ts->nTime / 1000) / 60 * 60);
            int slot = minute_to_slot(minute);
            if (slot < 0) continue;
            size_t idx = (size_t)c * NSLOT + slot;
            vol[idx] += ts->nVolume;
            amt[idx] += ts->nTurnover;
            nts[idx]++;
            if (is_buy_flag(ts->chBSFlag)) bvol[idx] += ts->nVolume;
            else if (is_sell_flag(ts->chBSFlag)) svol[idx] += ts->nVolume;
        }
    }

    // 缺失分钟前向填充（与 panel_tool 一致）
    long long missing = 0;
    for (int c = 0; c < N; ++c) {
        int first = -1;
        for (int s = 0; s < NSLOT; ++s) if (has[(size_t)c * NSLOT + s]) { first = s; break; }
        if (first < 0) { fprintf(stderr, "stock %d has no data\n", c); continue; }
        for (int s = first - 1; s >= 0; --s) {
            size_t i = (size_t)c * NSLOT + s, j = (size_t)c * NSLOT + first;
            op[i] = hi[i] = lo[i] = cl[i] = cl[j];
            bid1[i] = bid1[j]; ask1[i] = ask1[j];
            bv1[i] = bv1[j]; av1[i] = av1[j];
            tbid[i] = tbid[j]; task[i] = task[j];
            limU[i] = limU[j]; limD[i] = limD[j];
            ntime[i] = slot_to_minute(s) * 1000;
            ++missing;
        }
        float prev = cl[(size_t)c * NSLOT + first];
        for (int s = first + 1; s < NSLOT; ++s) {
            size_t i = (size_t)c * NSLOT + s;
            if (!has[i]) {
                size_t j = i - 1;
                op[i] = hi[i] = lo[i] = cl[i] = prev;
                bid1[i] = bid1[j]; ask1[i] = ask1[j];
                bv1[i] = bv1[j]; av1[i] = av1[j];
                tbid[i] = tbid[j]; task[i] = task[j];
                limU[i] = limU[j]; limD[i] = limD[j];
                ntime[i] = slot_to_minute(s) * 1000;
                ++missing;
            } else {
                prev = cl[i];
            }
        }
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/%d.panelbook", outdir.c_str(), date);
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
    std::vector<PanelBookRow> buf(N);
    long long rows = 0;
    for (int s = 0; s < NSLOT; ++s) {
        for (int c = 0; c < N; ++c) {
            size_t i = (size_t)c * NSLOT + s;
            PanelBookRow& r = buf[c];
            r.nTime = ntime[i];
            r.minute = slot_to_minute(s);
            r.cidx = c;
            r.nMd = nmd[i];
            r.nTs = nts[i];
            r.open = op[i]; r.high = hi[i]; r.low = lo[i]; r.close = cl[i];
            r.volume = (float)vol[i];
            r.amount = (float)amt[i];
            r.vwap = (vol[i] > 0.0 && amt[i] > 0.0) ? (float)(amt[i] / vol[i]) : cl[i];
            r.bid1 = bid1[i]; r.ask1 = ask1[i];
            r.bidvol1 = bv1[i]; r.askvol1 = av1[i];
            r.totbid = tbid[i]; r.totask = task[i];
            r.buyvol = (float)bvol[i]; r.sellvol = (float)svol[i];
            r.ofi = (float)ofi[i];
            r.limitUp = limU[i]; r.limitDown = limD[i];
        }
        fwrite(&buf[0], sizeof(PanelBookRow), N, f);
        rows += N;
    }
    fclose(f);
    printf("panelbook date=%d nTicks=%d rows=%lld stocks=%d missing_filled=%lld\n",
           date, nTicks, rows, N, missing);
    return 0;
}
