// 解析 TickPool 的索引结构：文件头、5 张描述符表、chunk 指针。
// 目标：确认「按 channel + chunk 指针直接读记录」是否可行。
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

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

static const size_t DESC_SIZE = 8008;      // 1000 指针 + 4 字节计数
static const int DESC_PTRS = 1000;

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <file.dat> [channel]\n", argv[0]); return 1; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 1; }
    int ch = (argc > 2) ? atoi(argv[2]) : 0;

    unsigned char hdr[64];
    if (fread(hdr, 1, 64, f) != 64) { fprintf(stderr, "short header\n"); return 1; }
    int64_t v[8];
    memcpy(v, hdr, 64);
    printf("header: n0=%lld n1=%lld\n", (long long)v[0], (long long)v[1]);
    printf("  offsets: ");
    for (int i = 2; i < 7; ++i) printf("%lld ", (long long)v[i]);
    printf("\n  v7=%lld\n", (long long)v[7]);

    int64_t nCh = v[1];
    if (nCh <= 0 || nCh > 20000) { fprintf(stderr, "bad channel count %lld\n", (long long)nCh); return 1; }

    const char* typeName[5] = {"table0", "table1", "table2", "table3", "table4"};
    for (int t = 0; t < 5; ++t) {
        int64_t base = v[2 + t];
        if (base <= 0) continue;
        // 读该表 channel=ch 的描述符
        long long off = base + (long long)ch * (long long)DESC_SIZE;
        if (fseek(f, off, SEEK_SET) != 0) continue;
        std::vector<unsigned char> buf(DESC_SIZE);
        if (fread(&buf[0], 1, DESC_SIZE, f) != DESC_SIZE) continue;
        int64_t ptrs[4];
        memcpy(ptrs, &buf[0], 32);
        int32_t cnt;
        memcpy(&cnt, &buf[8000], 4);
        printf("%s base=%lld ch=%d count=%d ptrs=%lld,%lld,%lld,%lld\n",
               typeName[t], (long long)base, ch, cnt,
               (long long)ptrs[0], (long long)ptrs[1], (long long)ptrs[2], (long long)ptrs[3]);
        // 若第一页指针有效，试读第一条记录
        if (ptrs[0] > 0 && ptrs[0] < 24LL * 1024 * 1024 * 1024LL) {
            unsigned char rec[256];
            if (fseek(f, (long)ptrs[0], SEEK_SET) == 0 && fread(rec, 1, 256, f) == 256) {
                if (t == 0) {
                    QuantMarketData md; memcpy(&md, rec, sizeof(md));
                    printf("    [as MD] nTime=%d bid1=%u/%u ask1=%u/%u\n",
                           md.nTime, md.nBidPrice[0], md.nBidVol[0], md.nAskPrice[0], md.nAskVol[0]);
                } else if (t == 1) {
                    QuantTransaction ts; memcpy(&ts, rec, sizeof(ts));
                    printf("    [as TS] nTime=%d px=%d vol=%d bs=%c fc=%c\n",
                           ts.nTime, ts.nPrice, ts.nVolume, ts.chBSFlag, ts.chFunctionCode);
                } else if (t == 2) {
                    QuantTdfOrder od; memcpy(&od, rec, sizeof(od));
                    printf("    [as OD] nTime=%d typ=%c bs=%c px=%d vol=%d\n",
                           od.nTime, od.nType, od.chBSFlag, od.nPrice, od.nVolume);
                }
            }
        }
    }

    // 统计各表全部 channel 的计数总和
    for (int t = 0; t < 5; ++t) {
        int64_t base = v[2 + t];
        if (base <= 0) continue;
        long long total = 0;
        for (int64_t c = 0; c < nCh; ++c) {
            int32_t cnt = 0;
            if (fseek(f, base + c * (int64_t)DESC_SIZE + 8000, SEEK_SET) != 0) break;
            if (fread(&cnt, 4, 1, f) != 1) break;
            if (cnt > 0) total += cnt;
        }
        printf("table%d total_count=%lld\n", t, total);
    }
    fclose(f);
    return 0;
}
