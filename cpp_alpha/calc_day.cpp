#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <chrono>
#include <cmath>
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
#pragma pack(pop)

static_assert(sizeof(QuantMarketData) == 256, "MD struct size mismatch");
static_assert(sizeof(QuantTransaction) == 32, "TS struct size mismatch");
static_assert(sizeof(QuantTdfOrder) == 20, "OD struct size mismatch");

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
    while (fgets(line, sizeof(line), f)) {
        v.push_back(atoi(line));
    }
    fclose(f);
    return v;
}

struct FactorState {
    int n;
    std::vector<double> a1, a2, a3, a4, a5;
    std::vector<std::array<double, 10> > b3, b4, b5;
    std::vector<int> c3, c4, c5;

    explicit FactorState(int nStocks)
        : n(nStocks),
          a1(nStocks, 0.0), a2(nStocks, 0.0), a3(nStocks, 0.0),
          a4(nStocks, 0.0), a5(nStocks, 0.0),
          b3(nStocks), b4(nStocks), b5(nStocks),
          c3(nStocks, 0), c4(nStocks, 0), c5(nStocks, 0) {
        for (int i = 0; i < nStocks; ++i) {
            b3[i].fill(0.0);
            b4[i].fill(0.0);
            b5[i].fill(0.0);
        }
    }

    inline void update_md(int c, const QuantMarketData* md) {
        // Alpha_001
        {
            double askVol = (double)md->nAskVol[0] + (double)md->nAskVol[1];
            double bidVol = (double)md->nBidVol[0] + (double)md->nBidVol[1];
            a1[c] = (bidVol != 0.0) ? (askVol / bidVol * 100.0) : 0.0;
        }
        // Alpha_002
        {
            double vol0 = (double)md->nAskVol[0] + (double)md->nBidVol[0];
            double volsum = vol0 + (double)md->nAskVol[1] + (double)md->nBidVol[1]
                          + (double)md->nAskVol[2] + (double)md->nBidVol[2];
            if (volsum != 0.0) {
                a2[c] = vol0 / volsum * 100.0;
            }
        }
        // Alpha_003
        if (md->nTotalAskVol > 0) {
            double v = (double)md->nTotalAskVol;
            std::array<double, 10>& b = b3[c];
            for (int k = 9; k > 0; --k) b[k] = b[k - 1];
            b[0] = v;
            ++c3[c];
            if (c3[c] > 10) {
                double mx = b[0], mn = b[0];
                for (int k = 1; k < 10; ++k) {
                    if (b[k] > mx) mx = b[k];
                    if (b[k] < mn) mn = b[k];
                }
                a3[c] = (mx / mn - 1.0) * 100.0;
            }
        }
    }

    inline void update_ts(int c, const QuantTransaction* ts) {
        if (ts->nVolume > 0) {
            std::array<double, 10>& b = b4[c];
            double den = b[8];
            for (int k = 9; k > 0; --k) b[k] = b[k - 1];
            b[0] = (double)ts->nVolume;
            ++c4[c];
            if (c4[c] > 10) {
                a4[c] = (double)ts->nVolume / den - 1.0;
            }
        }
    }

    inline void update_od(int c, const QuantTdfOrder* od) {
        if (od->nVolume > 0) {
            std::array<double, 10>& b = b5[c];
            double den = b[8];
            for (int k = 9; k > 0; --k) b[k] = b[k - 1];
            b[0] = (double)od->nVolume;
            ++c5[c];
            if (c5[c] > 10) {
                a5[c] = (double)od->nVolume / den - 1.0;
            }
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <YYYYMMDD> <out_dir>\n", argv[0]);
        return 1;
    }
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
    int nStocks = (int)tkrs.size();
    FactorState f(nStocks);
    std::vector<int> timeRecords(nStocks, 0);

    char mktPath[1024], datPath[1024], infoPath[1024];
    snprintf(mktPath, sizeof(mktPath), "%s/%d.mkt", outdir.c_str(), date);
    snprintf(datPath, sizeof(datPath), "%s/%d.dat", outdir.c_str(), date);
    snprintf(infoPath, sizeof(infoPath), "%s/alphaInfo.txt", outdir.c_str());

    FILE* fm = fopen(mktPath, "wb");
    FILE* fa = fopen(datPath, "wb");
    if (!fm || !fa) {
        fprintf(stderr, "cannot open output files\n");
        return 1;
    }

    FILE* fi = fopen(infoPath, "w");
    if (fi) {
        fprintf(fi, "%d,5\n", nStocks);
        fprintf(fi, "aID_001\naID_002\naID_003\naID_004\naID_005\n");
        fclose(fi);
    }

    const int limit = nTicks - 500;
    long long rows = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < limit; ++i) {
        TickResult t = get_tick(i);
        int c = t.cidx;
        int ty = t.type;

        if (ty == 0) {
            const QuantMarketData* md = (const QuantMarketData*)t.p;
            f.update_md(c, md);

            if (md->nAskPrice[0] > 0 || md->nBidPrice[0] > 0) {
                int minute = s_to_hhmmss(hhmmss_to_s(md->nTime / 1000) / 60 * 60);
                if (minute > timeRecords[c]) {
                    if ((minute >= 93000 && minute < 113000) ||
                        (minute >= 130000 && minute < 150000)) {
                        double midprc = ((double)md->nAskPrice[0] + (double)md->nBidPrice[0]) / 2.0;
                        if (md->nAskPrice[0] <= 0) midprc = (double)md->nBidPrice[0];
                        if (md->nBidPrice[0] <= 0) midprc = (double)md->nAskPrice[0];

                        int32_t mktRow[4] = {
                            (int32_t)md->nTime,
                            (int32_t)minute,
                            (int32_t)tkrs[c],
                            (int32_t)midprc
                        };
                        fwrite(mktRow, sizeof(int32_t), 4, fm);

                        float alphaRow[5] = {
                            (float)f.a1[c],
                            (float)f.a2[c],
                            (float)f.a3[c],
                            (float)f.a4[c],
                            (float)f.a5[c]
                        };
                        fwrite(alphaRow, sizeof(float), 5, fa);

                        timeRecords[c] = minute;
                        ++rows;
                    }
                }
            }
        } else if (ty == 1) {
            f.update_ts(c, (const QuantTransaction*)t.p);
        } else if (ty == 2) {
            f.update_od(c, (const QuantTdfOrder*)t.p);
        }
    }
    auto t1 = std::chrono::steady_clock::now();

    fclose(fm);
    fclose(fa);

    double sec = std::chrono::duration<double>(t1 - t0).count();
    printf("date=%d nTicks=%d limit=%d rows=%lld elapsed=%.1fs\n",
           date, nTicks, limit, rows, sec);
    return 0;
}
