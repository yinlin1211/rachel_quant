// fast_reader.cpp —— 按股票索引读取 TickPool，直接聚合分钟面板
//
// 旧读法：liblf_tick2py.so 的 get_tick(idx) 全局顺序扫描 4.37 亿条，再按股票/分钟
//         分桶排序。实测单日 172 秒。
// 新读法：liblquant_kernel.so 的 lquant_tick_pool 按 cidx 随机访问
//         get_md(cidx, i) / get_ts(cidx, i)，每只股票的三类子流本身时间有序，
//         无需排序，边读边聚合。
//
// 输出格式与 pipeline/panels/<date>.panel 完全一致：
//   packed 48B = int32 nTime, minute, cidx, nMd, nTs;
//                float open, high, low, close, vwap, volume, amount
//   行序：240 个分钟（外层）× N 只股票（内层）
//
// 口径（与 pipeline/src/panel_tool.cpp 对齐）：
//   - OHLC 取 MD 买卖一档中间价；bid1、ask1 都 <=0 的 MD 丢弃
//   - volume/amount 取 TS 的 nVolume / nTurnover
//   - 只保留 09:30-11:29、13:00-14:59，240 个分钟槽
//   - 缺失分钟用前收盘前向填充（开盘段用首个有效分钟回填），nMd/nTs 记 0
//
// 编译（必须定义 LQUANTSELFLog::logger 并链接 libdl）：
//   g++ -O2 -std=c++11 -o out/fast_reader src/fast_reader.cpp \
//       -I/opt/datas/lf_basket_trade/include \
//       -I/opt/datas/lf_basket_trade/include/kernel \
//       -L/opt/datas/lf_basket_trade/lib -llquant_kernel -ldl -lpthread \
//       -Wl,-rpath,/opt/datas/lf_basket_trade/lib
//
// 用法：
//   out/fast_reader [YYYYMMDD] [输出目录]
//   默认 20260105 / <项目>/out

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <limits>
#include <chrono>
#include <cerrno>
#include <sys/stat.h>

#include "lquant_tick_pool.hpp"
#include "lquant_log.hpp"

// liblquant_kernel.so 引用了这个静态成员，但库里没有定义；由使用方提供。
std::shared_ptr<lquantlog::logger> LQUANTSELFLog::logger;

#pragma pack(push, 1)
struct PanelRow {
    int32_t nTime, minute, cidx, nMd, nTs;
    float open, high, low, close, vwap, volume, amount;
};
#pragma pack(pop)
static_assert(sizeof(PanelRow) == 48, "PanelRow must be 48 bytes");

static const int NSLOT = 240;

static inline int hhmmss_to_s(int x) {
    return x / 10000 * 3600 + (x / 100 % 100) * 60 + x % 100;
}
static inline int s_to_hhmmss(int s) {
    return s / 3600 * 10000 + (s % 3600 / 60) * 100 + s % 60;
}
// 返回 0..239；非交易时段返回 -1
static inline int minute_to_slot(int minute) {
    int hh = minute / 10000, mm = minute / 100 % 100;
    int m = hh * 60 + mm;
    if (m >= 570 && m < 690) return m - 570;          // 09:30-11:29
    if (m >= 780 && m < 900) return 120 + (m - 780);  // 13:00-14:59
    return -1;
}
static inline int slot_to_minute(int slot) {
    int m = (slot < 120) ? (570 + slot) : (780 + slot - 120);
    return m / 60 * 10000 + m % 60 * 100;
}
// HHMMSSmmm -> 分钟槽，非交易时段返回 -1
static inline int time_to_slot(int nTime) {
    int minute = s_to_hhmmss(hhmmss_to_s(nTime / 1000) / 60 * 60);
    return minute_to_slot(minute);
}

static int count_universe(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return -1;
    int n = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) ++n;
    fclose(f);
    return n;
}

static double now_sec() {
    using namespace std::chrono;
    return duration_cast<duration<double> >(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    int date = argc > 1 ? atoi(argv[1]) : 20260105;
    std::string outdir = argc > 2 ? argv[2] : "out";
    char tickpath[512], outpath[1024];
    snprintf(tickpath, sizeof(tickpath), "/opt/bar_data/TickPool/%d.dat", date);

    int N = count_universe("/opt/bar_data/Meta/tkr.txt");
    if (N <= 0) {
        fprintf(stderr, "cannot read universe /opt/bar_data/Meta/tkr.txt\n");
        return 1;
    }

    LQUANTSELFLog::logger = lquantlog::stdout_color_mt("console");
    LQUANTSELFLog::logger->set_level(lquantlog::level::warn);

    double t0 = now_sec();
    lquant_tick_pool pool;
    if (!pool.load(tickpath)) {
        fprintf(stderr, "failed to load %s\n", tickpath);
        return 1;
    }
    double t_load = now_sec() - t0;

    if (mkdir(outdir.c_str(), 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "mkdir %s failed: %s\n", outdir.c_str(), strerror(errno));
        return 1;
    }

    const float NANV = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> op((size_t)N * NSLOT, NANV), hi((size_t)N * NSLOT, NANV);
    std::vector<float> lo((size_t)N * NSLOT, NANV), cl((size_t)N * NSLOT, NANV);
    std::vector<double> vol((size_t)N * NSLOT, 0.0), amt((size_t)N * NSLOT, 0.0);
    std::vector<int32_t> ntime((size_t)N * NSLOT, 0), nmd((size_t)N * NSLOT, 0),
                        nts((size_t)N * NSLOT, 0);
    std::vector<unsigned char> has((size_t)N * NSLOT, 0);

    long long md_read = 0, md_used = 0, ts_read = 0, ts_used = 0;

    // ---- 1) 按股票读 MD，聚合中间价 OHLC ----
    double t1 = now_sec();
    for (int c = 0; c < N; ++c) {
        const int n = pool.get_md_count(c);
        for (int i = 0; i < n; ++i) {
            const lquant_market_data_t* md = pool.get_md(c, i);
            ++md_read;
            if (md->nBidPrice[0] <= 0 && md->nAskPrice[0] <= 0) continue;
            int slot = time_to_slot(md->nTime);
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
            nmd[idx]++;
            ++md_used;
        }
    }
    double t_md = now_sec() - t1;

    // ---- 2) 按股票读 TS，聚合成交量/成交额 ----
    double t2 = now_sec();
    for (int c = 0; c < N; ++c) {
        const int n = pool.get_ts_count(c);
        for (int i = 0; i < n; ++i) {
            const lquant_transaction_t* ts = pool.get_ts(c, i);
            ++ts_read;
            int slot = time_to_slot(ts->nTime);
            if (slot < 0) continue;
            size_t idx = (size_t)c * NSLOT + slot;
            vol[idx] += ts->nVolume;
            amt[idx] += ts->nTurnover;
            nts[idx]++;
            ++ts_used;
        }
    }
    double t_ts = now_sec() - t2;

    // ---- 3) 缺失分钟填充（与 panel_tool.cpp 相同）----
    double t3 = now_sec();
    long long missing = 0, no_data = 0;
    for (int c = 0; c < N; ++c) {
        int first = -1, last = -1;
        for (int s = 0; s < NSLOT; ++s) {
            if (has[(size_t)c * NSLOT + s]) { if (first < 0) first = s; last = s; }
        }
        if (first < 0) { ++no_data; continue; }
        for (int s = first - 1; s >= 0; --s) {
            size_t i = (size_t)c * NSLOT + s, j = (size_t)c * NSLOT + first;
            op[i] = hi[i] = lo[i] = cl[i] = cl[j];
            ntime[i] = slot_to_minute(s) * 1000;
            ++missing;
        }
        float prev = cl[(size_t)c * NSLOT + first];
        for (int s = first + 1; s < NSLOT; ++s) {
            size_t i = (size_t)c * NSLOT + s;
            if (!has[i]) {
                op[i] = hi[i] = lo[i] = cl[i] = prev;
                ntime[i] = slot_to_minute(s) * 1000;
                ++missing;
            } else {
                prev = cl[i];
            }
        }
    }
    double t_fill = now_sec() - t3;

    // ---- 4) 按 slot 外层、cidx 内层写出 ----
    double t4 = now_sec();
    snprintf(outpath, sizeof(outpath), "%s/%d.fast.panel", outdir.c_str(), date);
    FILE* f = fopen(outpath, "wb");
    if (!f) {
        fprintf(stderr, "cannot open %s: %s\n", outpath, strerror(errno));
        return 1;
    }
    std::vector<PanelRow> buf(N);
    for (int s = 0; s < NSLOT; ++s) {
        for (int c = 0; c < N; ++c) {
            size_t i = (size_t)c * NSLOT + s;
            PanelRow& r = buf[c];
            r.nTime = ntime[i];
            r.minute = slot_to_minute(s);
            r.cidx = c;
            r.nMd = nmd[i];
            r.nTs = nts[i];
            r.open = op[i];
            r.high = hi[i];
            r.low = lo[i];
            r.close = cl[i];
            r.volume = (float)vol[i];
            r.amount = (float)amt[i];
            r.vwap = (vol[i] > 0.0 && amt[i] > 0.0) ? (float)(amt[i] / vol[i]) : cl[i];
        }
        fwrite(&buf[0], sizeof(PanelRow), N, f);
    }
    fclose(f);
    double t_write = now_sec() - t4;
    double t_total = now_sec() - t0;

    long long total = md_read + ts_read;
    printf("date=%d stocks=%d\n", date, N);
    printf("records: md=%lld ts=%lld total=%lld (used md=%lld ts=%lld)\n",
           md_read, ts_read, total, md_used, ts_used);
    printf("load=%.3fs md=%.3fs ts=%.3fs fill=%.3fs write=%.3fs total=%.3fs\n",
           t_load, t_md, t_ts, t_fill, t_write, t_total);
    printf("throughput=%.2f M records/s (read), %.2f M records/s (used)\n",
           total / t_total / 1e6, (md_used + ts_used) / t_total / 1e6);
    printf("missing_filled=%lld no_data_stocks=%lld output=%s\n",
           missing, no_data, outpath);
    return 0;
}
