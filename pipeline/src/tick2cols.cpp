// 把一天的 TickPool 原始文件转成「按时间排序的列式文件」，同时顺带产出分钟面板。
//
// 为什么要转：
//   原始格式是"内存镜像"（定长 256/32/20 字节、未压缩、到达序），
//   直接读一天 23.9 GB、还要全量重排。列式 + 压缩后约 1.5~4 GB，且天然按时间有序。
//
// 产物（每个 10 分钟一个 chunk，便于分块解压与并行）：
//   <out>/<date>/chunk_XX.cols     列式二进制（见下方 COLS 格式）
//   <out>/<date>.panelbook         分钟面板（与 panel_book 相同的行格式）
//   <out>/<date>.meta              记录数、字段表
//
// COLS 格式：
//   magic "COLS1\0" | int32 nrec | int32 ncols
//   ncols × { int32 nameLen, name, int32 dtype }
//   然后按列连续存放数据（每列 nrec 个元素）
//   dtype: 0=int32 1=uint32 2=float32 3=int8
//
// 用法：tick2cols <YYYYMMDD> <out_dir> [max_ticks]
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
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
struct QuantTdfOrder {
    int32_t nTime, nIndex, nPrice, nVolume;
    char nType, chBSFlag, chResv[2];
};
struct PanelBookRow {
    int32_t nTime, minute, cidx, nMd, nTs;
    float open, high, low, close, vwap, volume, amount;
    float bid1, ask1, bidvol1, askvol1, totbid, totask;
    float buyvol, sellvol, ofi, limitUp, limitDown;
};
#pragma pack(pop)

static_assert(sizeof(QuantMarketData) == 256, "MD");
static_assert(sizeof(QuantTransaction) == 32, "TS");

static const int NSLOT = 240;
static const int CHUNK_MIN = 10;              // 每个 chunk 10 分钟

static inline int ms_of_day(int nTime) {
    int t = nTime / 1000;
    int h = t / 10000, m = t / 100 % 100, s = t % 100;
    return ((h * 60 + m) * 60 + s) * 1000 + (nTime % 1000);
}
static inline int minute_of_ms(int ms) { return ms / 60000; }
static inline int slot_of_minute(int mn) {
    if (mn >= 570 && mn < 690) return mn - 570;
    if (mn >= 780 && mn < 900) return 120 + (mn - 780);
    return -1;
}
static inline int slot_to_minute(int slot) {
    int m = (slot < 120) ? (570 + slot) : (780 + slot - 120);
    return m / 60 * 10000 + m % 60 * 100;
}

// 三张表：行情快照 / 逐笔成交 / 逐笔委托（实盘要真实，OD 必须保留）
enum DType { DT_I32 = 0, DT_U32 = 1, DT_F32 = 2, DT_I8 = 3 };
struct ColDef { const char* name; int dtype; int width; };

static const ColDef MD_COLS[] = {
    {"time_ms", DT_I32, 4}, {"cidx", DT_I32, 4}, {"price", DT_U32, 4},
    {"bid1", DT_U32, 4}, {"ask1", DT_U32, 4}, {"bidv1", DT_U32, 4}, {"askv1", DT_U32, 4},
    {"bid2", DT_U32, 4}, {"ask2", DT_U32, 4}, {"bidv2", DT_U32, 4}, {"askv2", DT_U32, 4},
    {"bid3", DT_U32, 4}, {"ask3", DT_U32, 4}, {"bidv3", DT_U32, 4}, {"askv3", DT_U32, 4},
    {"bid4", DT_U32, 4}, {"ask4", DT_U32, 4}, {"bidv4", DT_U32, 4}, {"askv4", DT_U32, 4},
    {"bid5", DT_U32, 4}, {"ask5", DT_U32, 4}, {"bidv5", DT_U32, 4}, {"askv5", DT_U32, 4},
    {"bid6", DT_U32, 4}, {"ask6", DT_U32, 4}, {"bidv6", DT_U32, 4}, {"askv6", DT_U32, 4},
    {"bid7", DT_U32, 4}, {"ask7", DT_U32, 4}, {"bidv7", DT_U32, 4}, {"askv7", DT_U32, 4},
    {"bid8", DT_U32, 4}, {"ask8", DT_U32, 4}, {"bidv8", DT_U32, 4}, {"askv8", DT_U32, 4},
    {"bid9", DT_U32, 4}, {"ask9", DT_U32, 4}, {"bidv9", DT_U32, 4}, {"askv9", DT_U32, 4},
    {"bid10", DT_U32, 4}, {"ask10", DT_U32, 4}, {"bidv10", DT_U32, 4}, {"askv10", DT_U32, 4},
    {"totbid", DT_U32, 4}, {"totask", DT_U32, 4},
    {"preclose", DT_U32, 4}, {"limitup", DT_U32, 4}, {"limitdown", DT_U32, 4},
    {"ntrades", DT_U32, 4},
};
static const ColDef TS_COLS[] = {
    {"time_ms", DT_I32, 4}, {"cidx", DT_I32, 4}, {"price", DT_U32, 4},
    {"vol", DT_I32, 4}, {"turnover", DT_I32, 4}, {"bs", DT_I8, 1},
};
static const ColDef OD_COLS[] = {
    {"time_ms", DT_I32, 4}, {"cidx", DT_I32, 4}, {"price", DT_U32, 4},
    {"vol", DT_I32, 4}, {"otype", DT_I8, 1}, {"bs", DT_I8, 1},
};
static const int N_MD = (int)(sizeof(MD_COLS) / sizeof(MD_COLS[0]));
static const int N_TS = (int)(sizeof(TS_COLS) / sizeof(TS_COLS[0]));
static const int N_OD = (int)(sizeof(OD_COLS) / sizeof(OD_COLS[0]));

// 三张表的列缓冲
static std::vector<std::vector<unsigned char> > mdBuf, tsBuf, odBuf;

static void write_cols_file(const std::string& path, const ColDef* defs, int ncol,
                            std::vector<std::vector<unsigned char> >& buf, int nrec) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path.c_str()); exit(1); }
    fwrite("COLS1\0", 1, 6, f);
    fwrite(&nrec, 4, 1, f);
    fwrite(&ncol, 4, 1, f);
    for (int c = 0; c < ncol; ++c) {
        int len = (int)strlen(defs[c].name);
        fwrite(&len, 4, 1, f);
        fwrite(defs[c].name, 1, len, f);
        fwrite(&defs[c].dtype, 4, 1, f);
    }
    for (int c = 0; c < ncol; ++c)
        if (nrec > 0) fwrite(&buf[c][0], 1, (size_t)nrec * defs[c].width, f);
    fclose(f);
}

template <typename T>
static inline void append_val(std::vector<unsigned char>& v, T x) {
    v.insert(v.end(), (unsigned char*)&x, (unsigned char*)&x + sizeof(T));
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <YYYYMMDD> <out_dir> [max_ticks]\n", argv[0]); return 1; }
    int date = atoi(argv[1]);
    std::string outdir = argv[2];
    long long maxTicks = (argc > 3) ? atoll(argv[3]) : 0;
    if (mkdir(outdir.c_str(), 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir failed: %s\n", strerror(errno)); return 1;
    }
    char daydir[1024];
    snprintf(daydir, sizeof(daydir), "%s/%d", outdir.c_str(), date);
    if (mkdir(daydir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir %s failed\n", daydir); return 1;
    }

    std::string tickPath = "/opt/bar_data";
    std::vector<char> pb(tickPath.begin(), tickPath.end()); pb.push_back('\0');
    init_tick_file(&pb[0], date);
    int nTicks = get_tick_count();
    if (maxTicks > 0 && maxTicks < nTicks) nTicks = (int)maxTicks;

    // ---- 第一遍：按分钟分桶（文件不是时间有序的）----
    struct Ev { int ms; int k; };
    std::vector<std::vector<Ev> > bucket(NSLOT);
    long long indexed = 0;
    for (int k = 0; k < nTicks; ++k) {
        TickResult t = get_tick(k);
        if (t.type < 0 || t.type > 2) continue;
        int nt;
        if (t.type == 0) nt = ((const QuantMarketData*)t.p)->nTime;
        else if (t.type == 1) nt = ((const QuantTransaction*)t.p)->nTime;
        else nt = ((const QuantTdfOrder*)t.p)->nTime;
        int slot = slot_of_minute(minute_of_ms(ms_of_day(nt)));
        if (slot < 0) continue;
        Ev e; e.ms = ms_of_day(nt); e.k = k;
        bucket[slot].push_back(e);
        ++indexed;
    }
    fprintf(stderr, "indexed %lld market/trade ticks\n", indexed);

    // 分钟面板累加器
    std::vector<int> N(1, 0);   // 股票数在下面取
    // ---- 第二遍：逐分钟排序、写三张列式表、累加分钟面板 ----
    mdBuf.resize(N_MD); tsBuf.resize(N_TS); odBuf.resize(N_OD);
    long long written = 0, recMd = 0, recTs = 0, recOd = 0;
    int nStocks = 0;
    {
        std::string file = tickPath + "/Meta/tkr.txt";
        FILE* f = fopen(file.c_str(), "r");
        if (!f) { fprintf(stderr, "no tkr list\n"); return 1; }
        char line[128];
        while (fgets(line, sizeof(line), f)) ++nStocks;
        fclose(f);
    }
    std::vector<float> pOp((size_t)nStocks * NSLOT, std::nanf("")), pHi((size_t)nStocks * NSLOT, std::nanf("")),
                       pLo((size_t)nStocks * NSLOT, std::nanf("")), pCl((size_t)nStocks * NSLOT, std::nanf(""));
    std::vector<double> pVol((size_t)nStocks * NSLOT, 0.0), pAmt((size_t)nStocks * NSLOT, 0.0);
    std::vector<float> pBid((size_t)nStocks * NSLOT, std::nanf("")), pAsk((size_t)nStocks * NSLOT, std::nanf(""));
    std::vector<float> pBv((size_t)nStocks * NSLOT, 0.f), pAv((size_t)nStocks * NSLOT, 0.f);
    std::vector<float> pTb((size_t)nStocks * NSLOT, 0.f), pTa((size_t)nStocks * NSLOT, 0.f);
    std::vector<double> pBuy((size_t)nStocks * NSLOT, 0.0), pSell((size_t)nStocks * NSLOT, 0.0);
    std::vector<float> pLu((size_t)nStocks * NSLOT, std::nanf("")), pLd((size_t)nStocks * NSLOT, std::nanf(""));
    std::vector<int32_t> pNmd((size_t)nStocks * NSLOT, 0), pNts((size_t)nStocks * NSLOT, 0);
    std::vector<unsigned char> pHas((size_t)nStocks * NSLOT, 0);

    int chunk = -1;
    long long chunkMd = 0, chunkTs = 0, chunkOd = 0;

    auto flush_chunk = [&](int ci) {
        char path[1024];
        if (chunkMd > 0) {
            snprintf(path, sizeof(path), "%s/chunk_%02d.md.cols", daydir, ci);
            write_cols_file(path, MD_COLS, N_MD, mdBuf, (int)chunkMd);
        }
        if (chunkTs > 0) {
            snprintf(path, sizeof(path), "%s/chunk_%02d.ts.cols", daydir, ci);
            write_cols_file(path, TS_COLS, N_TS, tsBuf, (int)chunkTs);
        }
        if (chunkOd > 0) {
            snprintf(path, sizeof(path), "%s/chunk_%02d.od.cols", daydir, ci);
            write_cols_file(path, OD_COLS, N_OD, odBuf, (int)chunkOd);
        }
        written += chunkMd + chunkTs + chunkOd;
        recMd += chunkMd; recTs += chunkTs; recOd += chunkOd;
        for (int c = 0; c < N_MD; ++c) mdBuf[c].clear();
        for (int c = 0; c < N_TS; ++c) tsBuf[c].clear();
        for (int c = 0; c < N_OD; ++c) odBuf[c].clear();
        chunkMd = chunkTs = chunkOd = 0;
    };

    for (int slot = 0; slot < NSLOT; ++slot) {
        int ci = slot / CHUNK_MIN;
        if (ci != chunk) {
            if (chunk >= 0) flush_chunk(chunk);
            chunk = ci;
        }
        if (bucket[slot].empty()) { std::vector<Ev>().swap(bucket[slot]); continue; }
        std::sort(bucket[slot].begin(), bucket[slot].end(),
                  [](const Ev& a, const Ev& b) { return a.ms < b.ms; });

        for (size_t ei = 0; ei < bucket[slot].size(); ++ei) {
            const Ev& e = bucket[slot][ei];
            TickResult t = get_tick(e.k);
            int c = t.cidx;
            if (c < 0 || c >= nStocks) continue;
            size_t pi = (size_t)c * NSLOT + slot;

            if (t.type == 0) {
                const QuantMarketData* md = (const QuantMarketData*)t.p;
                if (md->nBidPrice[0] <= 0 && md->nAskPrice[0] <= 0) continue;
                uint32_t px = (md->nBidPrice[0] > 0 && md->nAskPrice[0] > 0)
                                  ? (md->nBidPrice[0] + md->nAskPrice[0]) / 2
                                  : md->nBidPrice[0] + md->nAskPrice[0];
                append_val(mdBuf[0], (int32_t)e.ms);
                append_val(mdBuf[1], (int32_t)c);
                append_val(mdBuf[2], px);
                for (int L = 0; L < 10; ++L) {
                    append_val(mdBuf[3 + 4 * L + 0], md->nBidPrice[L]);
                    append_val(mdBuf[3 + 4 * L + 1], md->nAskPrice[L]);
                    append_val(mdBuf[3 + 4 * L + 2], md->nBidVol[L]);
                    append_val(mdBuf[3 + 4 * L + 3], md->nAskVol[L]);
                }
                append_val(mdBuf[43], md->nTotalBidVol);
                append_val(mdBuf[44], md->nTotalAskVol);
                append_val(mdBuf[45], md->nPreClose);
                append_val(mdBuf[46], md->nHighLimited);
                append_val(mdBuf[47], md->nLowLimited);
                append_val(mdBuf[48], md->nNumTrades);
                ++chunkMd;

                if (!pHas[pi]) { pOp[pi] = pHi[pi] = pLo[pi] = pCl[pi] = (float)px; pHas[pi] = 1; }
                else {
                    if ((float)px > pHi[pi]) pHi[pi] = (float)px;
                    if ((float)px < pLo[pi]) pLo[pi] = (float)px;
                    pCl[pi] = (float)px;
                }
                pBid[pi] = (float)md->nBidPrice[0]; pAsk[pi] = (float)md->nAskPrice[0];
                pBv[pi] = (float)md->nBidVol[0]; pAv[pi] = (float)md->nAskVol[0];
                pTb[pi] = (float)md->nTotalBidVol; pTa[pi] = (float)md->nTotalAskVol;
                if (md->nHighLimited) pLu[pi] = (float)md->nHighLimited;
                if (md->nLowLimited) pLd[pi] = (float)md->nLowLimited;
                pNmd[pi]++;
            } else if (t.type == 1) {
                const QuantTransaction* ts = (const QuantTransaction*)t.p;
                if (ts->nPrice <= 0) continue;              // 撤单回报
                append_val(tsBuf[0], (int32_t)e.ms);
                append_val(tsBuf[1], (int32_t)c);
                append_val(tsBuf[2], (uint32_t)ts->nPrice);
                append_val(tsBuf[3], (int32_t)ts->nVolume);
                append_val(tsBuf[4], (int32_t)ts->nTurnover);
                tsBuf[5].push_back((unsigned char)ts->chBSFlag);
                ++chunkTs;

                pVol[pi] += ts->nVolume; pAmt[pi] += ts->nTurnover;
                if (ts->chBSFlag == 'B' || ts->chBSFlag == '1') pBuy[pi] += ts->nVolume;
                else if (ts->chBSFlag == 'S' || ts->chBSFlag == '2') pSell[pi] += ts->nVolume;
                pNts[pi]++;
                if (!pHas[pi]) { pOp[pi] = pHi[pi] = pLo[pi] = pCl[pi] = (float)ts->nPrice; pHas[pi] = 1; }
            } else {
                const QuantTdfOrder* od = (const QuantTdfOrder*)t.p;
                append_val(odBuf[0], (int32_t)e.ms);
                append_val(odBuf[1], (int32_t)c);
                append_val(odBuf[2], (uint32_t)od->nPrice);
                append_val(odBuf[3], (int32_t)od->nVolume);
                odBuf[4].push_back((unsigned char)od->nType);
                odBuf[5].push_back((unsigned char)od->chBSFlag);
                ++chunkOd;
            }
        }
        std::vector<Ev>().swap(bucket[slot]);
    }
    if (chunk >= 0) flush_chunk(chunk);

    // 写分钟面板
    char pp[1024];
    snprintf(pp, sizeof(pp), "%s/%d.panelbook", outdir.c_str(), date);
    FILE* fp = fopen(pp, "wb");
    if (fp) {
        std::vector<PanelBookRow> buf(nStocks);
        for (int s = 0; s < NSLOT; ++s) {
            for (int c = 0; c < nStocks; ++c) {
                size_t i = (size_t)c * NSLOT + s;
                PanelBookRow& r = buf[c];
                memset(&r, 0, sizeof(r));
                r.nTime = slot_to_minute(s) * 1000;
                r.minute = slot_to_minute(s);
                r.cidx = c;
                r.nMd = pNmd[i]; r.nTs = pNts[i];
                r.open = pOp[i]; r.high = pHi[i]; r.low = pLo[i]; r.close = pCl[i];
                r.volume = (float)pVol[i]; r.amount = (float)pAmt[i];
                r.vwap = (pVol[i] > 0) ? (float)(pAmt[i] / pVol[i]) : r.close;
                r.bid1 = pBid[i]; r.ask1 = pAsk[i];
                r.bidvol1 = pBv[i]; r.askvol1 = pAv[i];
                r.totbid = pTb[i]; r.totask = pTa[i];
                r.buyvol = (float)pBuy[i]; r.sellvol = (float)pSell[i];
                r.limitUp = pLu[i]; r.limitDown = pLd[i];
            }
            fwrite(&buf[0], sizeof(PanelBookRow), nStocks, fp);
        }
        fclose(fp);
    }

    char mp[1024];
    snprintf(mp, sizeof(mp), "%s/%d.meta", outdir.c_str(), date);
    FILE* fm = fopen(mp, "w");
    if (fm) {
        fprintf(fm, "date=%d ticks_indexed=%lld written=%lld md=%lld ts=%lld od=%lld stocks=%d ncols=%d/%d/%d\n",
                date, indexed, written, recMd, recTs, recOd, nStocks, N_MD, N_TS, N_OD);
        fclose(fm);
    }
    printf("tick2cols date=%d indexed=%lld md=%lld ts=%lld od=%lld stocks=%d\n", date, indexed, recMd, recTs, recOd, nStocks);
    return 0;
}
