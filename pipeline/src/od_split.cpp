// od_split.cpp —— E5：拆单与冰山单识别（从逐笔委托的订单号识别算法拆单）
//
// 思路：交易所对每笔报单分配「委托编号」nIndex。算法拆单的典型特征是：
//   同一价位、同一方向、短时间内连续报单，且委托编号常常连续。
// 因此构造三个可计算的代理特征（按 股票×分钟 聚合）：
//   n_orders      该分钟委托笔数
//   n_seq_same    与上一笔「同价、同向、间隔 < SEQ_WINDOW_MS」的笔数
//   n_idx_consec  与上一笔「委托编号连续（nIndex = prev + 1）」的笔数
// 拆单率 split_ratio = n_seq_same / n_orders
//
// 输出：date,cidx,slot,n_orders,n_seq_same,n_idx_consec
//
// 编译：
//   g++ -O3 -std=c++11 -o bin/od_split src/od_split.cpp \
//       /home/intern2/pyTickAlpha/liblf_tick2py.so \
//       -L/home/intern2/pyTickAlpha -llquant_kernel -lrt -lpthread \
//       -Wl,-rpath,/home/intern2/pyTickAlpha
//
// 用法：od_split <YYYYMMDD> <out.csv>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

struct TickResult { int cidx; int type; void* p; };
extern "C" {
    int  init_tick_file(char* path, int date);
    int  get_tick_count();
    TickResult get_tick(int idx);
}

struct QuantTdfOrder {
    int  nTime;
    int  nIndex;
    int  nPrice;
    int  nVolume;
    char nType;
    char chBSFlag;
    char chResv[2];
};

static const int NSLOT = 240;
static const int SEQ_WINDOW_MS = 5000;   // 「同一批拆单」的时间窗

// nTime 是 HHMMSSmmm 的十进制表示（如 93000000 = 9:30:00.000），
// 不是毫秒时间戳。先折算成「毫秒 of day」再分槽。
static inline int ms_of_day(int t) {
    int hh = t / 10000000;
    int mi = (t / 100000) % 100;
    int ss = (t / 1000) % 100;
    int ms = t % 1000;
    return ((hh * 60 + mi) * 60 + ss) * 1000 + ms;
}

static inline int slot_of_ms(int msOfDay) {
    int mod = msOfDay / 60000;
    if (mod >= 570 && mod < 690) return mod - 570;
    if (mod >= 780 && mod < 900) return 120 + (mod - 780);
    return -1;
}

struct Prev { int price, side, ms, idx; bool has; };

struct Acc { int n, seq, cons; };

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <YYYYMMDD> <out.csv>\n", argv[0]);
        return 1;
    }
    int date = atoi(argv[1]);
    const char* outPath = argv[2];

    char base[] = "/opt/bar_data";
    init_tick_file(base, date);
    int nTicks = get_tick_count();
    if (nTicks <= 0) { fprintf(stderr, "no ticks for %d\n", date); return 2; }
    fprintf(stderr, "date=%d ticks=%d\n", date, nTicks);

    // 股票数
    int nStocks = 0;
    {
        char p[256];
        snprintf(p, sizeof(p), "%s/Meta/tkr.txt", base);
        FILE* f = fopen(p, "r");
        if (!f) { fprintf(stderr, "no tkr list\n"); return 3; }
        char line[128];
        while (fgets(line, sizeof(line), f)) ++nStocks;
        fclose(f);
    }
    fprintf(stderr, "stocks=%d\n", nStocks);

    std::vector<Acc>  acc((size_t)nStocks * NSLOT);
    std::vector<Prev> prev(nStocks);
    for (int i = 0; i < nStocks; ++i) { prev[i].has = false; prev[i].ms = -1; }

    long long nOd = 0, nUsed = 0;
    for (int k = 0; k < nTicks; ++k) {
        TickResult t = get_tick(k);
        if (t.type != 2) continue;              // 只要 OD（逐笔委托）
        const QuantTdfOrder* od = (const QuantTdfOrder*)t.p;
        if (!od) continue;
        ++nOd;

        int c = t.cidx;
        if (c < 0 || c >= nStocks) continue;
        int tms = ms_of_day(od->nTime);
        int slot = slot_of_ms(tms);
        if (slot < 0) continue;

        Acc& a = acc[(size_t)c * NSLOT + slot];
        ++a.n; ++nUsed;

        int px = od->nPrice;
        int sd = (int)(unsigned char)od->chBSFlag;
        Prev& pv = prev[c];
        if (pv.has) {
            int gap = tms - pv.ms;
            if (gap >= 0 && gap < SEQ_WINDOW_MS && px == pv.price && sd == pv.side) ++a.seq;
            if (od->nIndex == pv.idx + 1) ++a.cons;
        }
        pv.price = px; pv.side = sd; pv.ms = tms; pv.idx = od->nIndex; pv.has = true;

        if ((k & 0x3FFFFFF) == 0) fprintf(stderr, "  scanned %d/%d\n", k, nTicks);
    }
    fprintf(stderr, "od=%lld used_in_session=%lld\n", nOd, nUsed);

    FILE* out = fopen(outPath, "w");
    if (!out) { fprintf(stderr, "cannot write %s\n", outPath); return 4; }
    fprintf(out, "date,cidx,slot,n_orders,n_seq_same,n_idx_consec\n");
    long long rows = 0;
    for (int c = 0; c < nStocks; ++c) {
        for (int s = 0; s < NSLOT; ++s) {
            const Acc& a = acc[(size_t)c * NSLOT + s];
            if (a.n == 0) continue;
            fprintf(out, "%d,%d,%d,%d,%d,%d\n", date, c, s, a.n, a.seq, a.cons);
            ++rows;
        }
    }
    fclose(out);
    printf("od_split date=%d od=%lld written=%lld rows=%lld\n", date, nOd, nUsed, rows);
    return 0;
}
