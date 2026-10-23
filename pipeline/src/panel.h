// 分钟面板的载入（Alpha101 与表达式因子共用）
#ifndef PIPELINE_PANEL_H
#define PIPELINE_PANEL_H

#include "common.h"

struct Panel {
    Mat open, high, low, close, vwap, volume, amount;
    int T, N;
};

static bool load_panel(const std::string& dir, const std::vector<int>& dates, Panel& p) {
    p.T = (int)dates.size() * 240;
    p.N = 0;
    std::vector<FILE*> fs(dates.size());
    std::vector<long long> nrows(dates.size());
    for (size_t d = 0; d < dates.size(); ++d) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%d.panel", dir.c_str(), dates[d]);
        FILE* f = fopen(path, "rb");
        if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }
        fseek(f, 0, SEEK_END);
        long long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        nrows[d] = sz / (long long)sizeof(PanelRow);
        if (p.N == 0) p.N = (int)(nrows[d] / 240);
        fs[d] = f;
    }
    if (p.N <= 0) return false;
    p.open = Mat(p.T, p.N); p.high = Mat(p.T, p.N); p.low = Mat(p.T, p.N);
    p.close = Mat(p.T, p.N); p.vwap = Mat(p.T, p.N);
    p.volume = Mat(p.T, p.N); p.amount = Mat(p.T, p.N);

    std::vector<PanelRow> buf(p.N);
    for (size_t d = 0; d < dates.size(); ++d) {
        if (nrows[d] != (long long)p.N * 240) {
            fprintf(stderr, "panel %d row count %lld != %d*240\n", dates[d], nrows[d], p.N);
            return false;
        }
        for (int s = 0; s < 240; ++s) {
            size_t got = fread(&buf[0], sizeof(PanelRow), p.N, fs[d]);
            if (got != (size_t)p.N) { fprintf(stderr, "short read %d\n", dates[d]); return false; }
            int t = (int)d * 240 + s;
            for (int i = 0; i < p.N; ++i) {
                const PanelRow& r = buf[i];
                p.open.at(i, t) = r.open;
                p.high.at(i, t) = r.high;
                p.low.at(i, t) = r.low;
                p.close.at(i, t) = r.close;
                p.vwap.at(i, t) = r.vwap;
                p.volume.at(i, t) = r.volume;
                p.amount.at(i, t) = r.amount;
            }
        }
        fclose(fs[d]);
    }
    return true;
}

#endif
