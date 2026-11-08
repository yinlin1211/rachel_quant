// 把 (symbol, 时间, score) 映射成 bt5 需要的全局 tick 下标 tidx。
// 用法: make_signals <YYYYMMDD> <scores.csv> <out.csv> [basket.csv]
// scores.csv: symbol,HHMMSSmmm,score
// out.csv:    didx,tidx,cidx,score
#include "lquant_tick_pool.hpp"
#include "lquant_log.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>

std::shared_ptr<lquantlog::logger> LQUANTSELFLog::logger;

struct Pending {
    int timeMs;
    float score;
    long outIdx;   // 写入输出时的顺序
};

static int toMs(int hhmmssmmm) {
    int h = hhmmssmmm / 10000000;
    int m = (hhmmssmmm / 100000) % 100;
    int s = (hhmmssmmm / 1000) % 100;
    int ms = hhmmssmmm % 1000;
    return ((h * 60 + m) * 60 + s) * 1000 + ms;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <YYYYMMDD> <scores.csv> <out.csv> [basket.csv]\n", argv[0]);
        return 1;
    }
    LQUANTSELFLog::logger = lquantlog::stdout_color_mt("console");

    int date = atoi(argv[1]);
    std::string scoreFile = argv[2];
    std::string outFile = argv[3];

    // 1) tkr -> cidx
    std::unordered_map<std::string, int> sym2cidx;
    {
        std::ifstream in("/opt/bar_data/Meta/tkr.txt");
        std::string line;
        int c = 0;
        while (std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            if (!line.empty()) sym2cidx[line] = c;
            ++c;
        }
    }

    // 2) 篮子过滤（可选）
    std::unordered_map<std::string, char> inBasket;
    if (argc >= 5) {
        std::ifstream in(argv[4]);
        std::string line;
        bool first = true;
        while (std::getline(in, line)) {
            if (first) { first = false; if (line.find("TKR") != std::string::npos) continue; }
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            size_t p = line.find(',');
            if (p != std::string::npos) inBasket[line.substr(0, p)] = 1;
        }
    }

    // 3) 读信号
    std::map<int, std::vector<Pending> > byCidx;
    {
        std::ifstream in(scoreFile);
        if (!in) { fprintf(stderr, "cannot open %s\n", scoreFile.c_str()); return 2; }
        std::string line;
        long n = 0;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t p1 = line.find(',');
            size_t p2 = line.find(',', p1 + 1);
            if (p1 == std::string::npos || p2 == std::string::npos) continue;
            std::string sym = line.substr(0, p1);
            if (!inBasket.empty() && !inBasket.count(sym)) continue;
            auto it = sym2cidx.find(sym);
            if (it == sym2cidx.end()) continue;
            int t = atoi(line.substr(p1 + 1, p2 - p1 - 1).c_str());
            float sc = (float)atof(line.substr(p2 + 1).c_str());
            Pending pd; pd.timeMs = toMs(t); pd.score = sc; pd.outIdx = n++;
            byCidx[it->second].push_back(pd);
        }
    }
    for (auto& kv : byCidx)
        std::sort(kv.second.begin(), kv.second.end(),
                  [](const Pending& a, const Pending& b) { return a.timeMs < b.timeMs; });

    long total = 0;
    for (auto& kv : byCidx) total += (long)kv.second.size();
    fprintf(stderr, "signals=%ld stocks=%zu\n", total, byCidx.size());
    if (total == 0) return 3;

    // 4) 打开 TickPool
    char path[512];
    snprintf(path, sizeof(path), "/opt/bar_data/TickPool/%08d.dat", date);
    lquant_tick_pool pool;
    if (!pool.load(path)) { fprintf(stderr, "load failed: %s\n", path); return 4; }
    long nTicks = pool.get_last_didx();
    fprintf(stderr, "ticks=%ld\n", nTicks);

    // 5) 单遍扫描，把每个信号挂到第一个时间 >= 目标的 tick 上
    std::vector<size_t> cursor(byCidx.size(), 0);
    // 用 cidx -> 在 byCidx 迭代顺序里的位置
    std::unordered_map<int, std::vector<Pending>*> ptr;
    std::unordered_map<int, size_t> pos;
    for (auto& kv : byCidx) { ptr[kv.first] = &kv.second; pos[kv.first] = 0; }

    std::vector<std::string> outLines;
    outLines.reserve((size_t)total);

    for (long k = 0; k <= nTicks; ++k) {
        auto r = pool.get_data(k);
        int c = (int)std::get<0>(r);
        int idx = (int)std::get<1>(r);
        GDataType type = std::get<2>(r);
        auto pit = ptr.find(c);
        if (pit == ptr.end()) continue;
        int t = 0;
        if (type == TYPE_MD) { auto* p = pool.get_md(c, idx); if (p) t = p->nTime; }
        else if (type == TYPE_TS) { auto* p = pool.get_ts(c, idx); if (p) t = p->nTime; }
        else if (type == TYPE_OD) { auto* p = pool.get_od(c, idx); if (p) t = p->nTime; }
        if (t <= 0) continue;
        int ms = toMs(t);
        std::vector<Pending>& vec = *pit->second;
        size_t& i = pos[c];
        while (i < vec.size() && vec[i].timeMs <= ms) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%ld,%ld,%d,%.6f", k, k, c, vec[i].score);
            outLines.push_back(buf);
            ++i;
        }
    }

    FILE* f = fopen(outFile.c_str(), "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", outFile.c_str()); return 5; }
    for (size_t i = 0; i < outLines.size(); ++i)
        fprintf(f, "%s\n", outLines[i].c_str());
    fclose(f);
    fprintf(stderr, "written=%zu/%ld\n", outLines.size(), total);
    return 0;
}
