#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>
#include <cerrno>
#include <sys/stat.h>

struct TickResult {
    int cidx;
    int type;
    void* p;
};

extern "C" {
int init_tick_file(char* path, int date);
int get_tick_count();
TickResult get_tick(int idx);
}

#pragma pack(push, 1)
struct QuantMarketData {
    int32_t gid;
    int32_t nTime;
    int32_t nStatus;
    uint32_t nPreClose;
    uint32_t nOpen;
    uint32_t nHigh;
    uint32_t nLow;
    uint32_t nMatch;
    uint32_t nAskPrice[10];
    uint32_t nAskVol[10];
    uint32_t nBidPrice[10];
    uint32_t nBidVol[10];
    uint32_t nNumTrades;
    int64_t iVolume;
    int64_t iTurnover;
    int64_t nTotalBidVol;
    int64_t nTotalAskVol;
    uint32_t nWeightedAvgBidPrice;
    uint32_t nWeightedAvgAskPrice;
    int32_t nIOPV;
    int32_t nYieldToMaturity;
    uint32_t nHighLimited;
    uint32_t nLowLimited;
    char chPrefix[4];
};

struct QuantTransaction {
    int32_t nTime;
    int32_t nIndex;
    int32_t nPrice;
    int32_t nVolume;
    int32_t nTurnover;
    char chBSFlag;
    char chOrderKind;
    char chFunctionCode;
    char chResv;
    int32_t nAskOrder;
    int32_t nBidOrder;
};

struct QuantTdfOrder {
    int32_t nTime;
    int32_t nIndex;
    int32_t nPrice;
    int32_t nVolume;
    char chType;
    char chBSFlag;
    char chResv[2];
};

struct CacheRow {
    int32_t nTime;
    int32_t minute;
    int32_t cidx;
    int32_t midprc;
    float bid1, ask1, bidvol1, askvol1;
    float bidvol2, askvol2, bidvol3, askvol3;
    float totalbidvol, totalaskvol, lastprice;
    float tsCnt, tsVol, tsBuy, tsSell;
    float odCnt, odVol, odBuy, odSell;
    float prevMid;
};
#pragma pack(pop)

static_assert(sizeof(QuantMarketData) == 256, "MD size");
static_assert(sizeof(QuantTransaction) == 32, "TS size");
static_assert(sizeof(QuantTdfOrder) == 20, "OD size");
static_assert(sizeof(CacheRow) == 96, "CacheRow size");

static inline int hhmmss_to_s(int x) {
    return x / 10000 * 3600 + (x / 100 % 100) * 60 + x % 100;
}
static inline int s_to_hhmmss(int s) {
    return s / 3600 * 10000 + (s % 3600 / 60) * 100 + s % 60;
}

static std::vector<int> load_tkr_list(const std::string& path) {
    std::vector<int> v;
    std::string file = path + "/Meta/tkr.txt";
    FILE* f = fopen(file.c_str(), "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", file.c_str());
        exit(1);
    }
    char line[128];
    while (fgets(line, sizeof(line), f)) v.push_back(atoi(line));
    fclose(f);
    return v;
}

struct Agg {
    int64_t tsCnt, tsVol, tsBuy, tsSell;
    int64_t odCnt, odVol, odBuy, odSell;
    Agg() : tsCnt(0), tsVol(0), tsBuy(0), tsSell(0),
            odCnt(0), odVol(0), odBuy(0), odSell(0) {}
};

static int cmd_cache(int date, const std::string& outdir) {
    if (mkdir(outdir.c_str(), 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir %s failed\n", outdir.c_str());
        return 1;
    }
    std::string tickPath = "/opt/bar_data";
    std::vector<char> pathBuf(tickPath.begin(), tickPath.end());
    pathBuf.push_back('\0');
    init_tick_file(&pathBuf[0], date);
    int nTicks = get_tick_count();

    std::vector<int> tkrs = load_tkr_list(tickPath);
    int nStocks = (int)tkrs.size();
    std::vector<Agg> agg(nStocks);
    std::vector<int> timeRecords(nStocks, 0);
    std::vector<double> lastMid(nStocks, 0.0);

    char mktPath[1024], cachePath[1024];
    snprintf(mktPath, sizeof(mktPath), "%s/%d.mkt", outdir.c_str(), date);
    snprintf(cachePath, sizeof(cachePath), "%s/%d.cache", outdir.c_str(), date);
    FILE* fm = fopen(mktPath, "wb");
    FILE* fc = fopen(cachePath, "wb");
    if (!fm || !fc) return 1;

    const int limit = nTicks - 500;
    long long rows = 0;
    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < limit; ++i) {
        TickResult t = get_tick(i);
        int c = t.cidx;
        int ty = t.type;

        if (ty == 0) {
            const QuantMarketData* md = (const QuantMarketData*)t.p;
            if (md->nAskPrice[0] > 0 || md->nBidPrice[0] > 0) {
                int minute = s_to_hhmmss(hhmmss_to_s(md->nTime / 1000) / 60 * 60);
                if (minute > timeRecords[c]) {
                    if ((minute >= 93000 && minute < 113000) ||
                        (minute >= 130000 && minute < 150000)) {
                        double midprc = ((double)md->nAskPrice[0] + (double)md->nBidPrice[0]) / 2.0;
                        if (md->nAskPrice[0] <= 0) midprc = (double)md->nBidPrice[0];
                        if (md->nBidPrice[0] <= 0) midprc = (double)md->nAskPrice[0];

                        int32_t mktRow[4] = {
                            (int32_t)md->nTime, (int32_t)minute,
                            (int32_t)tkrs[c], (int32_t)midprc
                        };
                        fwrite(mktRow, sizeof(int32_t), 4, fm);

                        CacheRow r;
                        memset(&r, 0, sizeof(r));
                        r.nTime = md->nTime;
                        r.minute = minute;
                        r.cidx = tkrs[c];
                        r.midprc = (int32_t)midprc;
                        r.bid1 = (float)md->nBidPrice[0];
                        r.ask1 = (float)md->nAskPrice[0];
                        r.bidvol1 = (float)md->nBidVol[0];
                        r.askvol1 = (float)md->nAskVol[0];
                        r.bidvol2 = (float)md->nBidVol[1];
                        r.askvol2 = (float)md->nAskVol[1];
                        r.bidvol3 = (float)md->nBidVol[2];
                        r.askvol3 = (float)md->nAskVol[2];
                        r.totalbidvol = (float)md->nTotalBidVol;
                        r.totalaskvol = (float)md->nTotalAskVol;
                        r.lastprice = (float)md->nMatch;
                        r.tsCnt = (float)agg[c].tsCnt;
                        r.tsVol = (float)agg[c].tsVol;
                        r.tsBuy = (float)agg[c].tsBuy;
                        r.tsSell = (float)agg[c].tsSell;
                        r.odCnt = (float)agg[c].odCnt;
                        r.odVol = (float)agg[c].odVol;
                        r.odBuy = (float)agg[c].odBuy;
                        r.odSell = (float)agg[c].odSell;
                        r.prevMid = (float)lastMid[c];
                        fwrite(&r, sizeof(CacheRow), 1, fc);

                        timeRecords[c] = minute;
                        lastMid[c] = midprc;
                        agg[c] = Agg();
                        ++rows;
                    }
                }
            }
        } else if (ty == 1) {
            const QuantTransaction* ts = (const QuantTransaction*)t.p;
            Agg& a = agg[c];
            ++a.tsCnt;
            a.tsVol += ts->nVolume;
            if (ts->chBSFlag == 'B') a.tsBuy += ts->nVolume;
            else if (ts->chBSFlag == 'S') a.tsSell += ts->nVolume;
        } else if (ty == 2) {
            const QuantTdfOrder* od = (const QuantTdfOrder*)t.p;
            Agg& a = agg[c];
            ++a.odCnt;
            a.odVol += od->nVolume;
            if (od->chBSFlag == 'B') a.odBuy += od->nVolume;
            else if (od->chBSFlag == 'S') a.odSell += od->nVolume;
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    fclose(fm);
    fclose(fc);
    double sec = std::chrono::duration<double>(t1 - t0).count();
    printf("cache date=%d nTicks=%d rows=%lld elapsed=%.1fs\n", date, nTicks, rows, sec);
    return 0;
}

static int cmd_compute(int date, const std::string& cachedir, const std::string& outdir) {
    if (mkdir(outdir.c_str(), 0755) != 0 && errno != EEXIST) return 1;
    char cachePath[1024], mktPath[1024], datPath[1024], infoPath[1024];
    snprintf(cachePath, sizeof(cachePath), "%s/%d.cache", cachedir.c_str(), date);
    snprintf(mktPath, sizeof(mktPath), "%s/%d.mkt", outdir.c_str(), date);
    snprintf(datPath, sizeof(datPath), "%s/%d.dat", outdir.c_str(), date);
    snprintf(infoPath, sizeof(infoPath), "%s/alphaInfo.txt", outdir.c_str());

    auto t0 = std::chrono::steady_clock::now();
    FILE* fc = fopen(cachePath, "rb");
    if (!fc) {
        fprintf(stderr, "cannot open %s\n", cachePath);
        return 1;
    }
    fseek(fc, 0, SEEK_END);
    long sz = ftell(fc);
    fseek(fc, 0, SEEK_SET);
    std::vector<CacheRow> rows(sz / sizeof(CacheRow));
    size_t got = fread(&rows[0], sizeof(CacheRow), rows.size(), fc);
    fclose(fc);

    FILE* fm = fopen(mktPath, "wb");
    FILE* fa = fopen(datPath, "wb");
    FILE* fi = fopen(infoPath, "w");
    if (fi) {
        fprintf(fi, "0,5\nnf_001\nnf_002\nnf_003\nnf_004\nnf_005\n");
        fclose(fi);
    }
    for (size_t i = 0; i < got; ++i) {
        const CacheRow& r = rows[i];
        int32_t mktRow[4] = {r.nTime, r.minute, r.cidx, r.midprc};
        fwrite(mktRow, sizeof(int32_t), 4, fm);

        float f1 = (r.askvol1 - r.bidvol1) / (r.askvol1 + r.bidvol1 + 1.0f);
        float f2 = (r.tsBuy - r.tsSell) / (r.tsBuy + r.tsSell + 1.0f);
        float f3 = (r.odBuy - r.odSell) / (r.odBuy + r.odSell + 1.0f);
        float f4 = (r.tsVol - r.odVol) / (r.tsVol + r.odVol + 1.0f);
        float f5 = (r.prevMid > 0.0f) ? (r.midprc / r.prevMid - 1.0f) : 0.0f;
        float a[5] = {f1, f2, f3, f4, f5};
        fwrite(a, sizeof(float), 5, fa);
    }
    fclose(fm);
    fclose(fa);
    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();
    printf("compute date=%d rows=%zu cache_mb=%.1f elapsed=%.2fs\n",
           date, got, sz / 1048576.0, sec);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage:\n  %s cache <date> <outdir>\n  %s compute <date> <cachedir> <outdir>\n",
                argv[0], argv[0]);
        return 1;
    }
    std::string mode = argv[1];
    int date = atoi(argv[2]);
    if (mode == "cache" && argc >= 4) return cmd_cache(date, argv[3]);
    if (mode == "compute" && argc >= 5) return cmd_compute(date, argv[3], argv[4]);
    fprintf(stderr, "bad arguments\n");
    return 1;
}
