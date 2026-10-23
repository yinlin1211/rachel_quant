// 表达式因子批量求值：读 factors.txt，逐个求值并统计 IC（不回写因子值）
#include "expr.h"
#include <omp.h>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

struct Stat { double n, sx, sy, sxy, sxx, syy; };

static void add_stat(Stat& s, double x, double y) {
    s.n += 1; s.sx += x; s.sy += y;
    s.sxy += x * y; s.sxx += x * x; s.syy += y * y;
}

static double stat_ic(const Stat& s) {
    if (s.n < 10) return NAN;
    double cov = s.n * s.sxy - s.sx * s.sy;
    double vx = s.n * s.sxx - s.sx * s.sx;
    double vy = s.n * s.syy - s.sy * s.sy;
    if (vx <= 0 || vy <= 0) return NAN;
    return cov / std::sqrt(vx * vy);
}

int main(int argc, char** argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s <panel_dir> <factors.txt> <out.csv> <date1> [date2 ...]\n", argv[0]);
        return 1;
    }
    std::string panelDir = argv[1];
    std::string factorFile = argv[2];
    std::string outCsv = argv[3];
    std::vector<int> dates;
    for (int i = 4; i < argc; ++i) dates.push_back(atoi(argv[i]));

    Panel p;
    auto t0 = std::chrono::steady_clock::now();
    if (!load_panel(panelDir, dates, p)) return 1;
    fprintf(stderr, "panel T=%d N=%d\n", p.T, p.N);

    const int nd = (int)dates.size();
    Mat ret(p.T, p.N);
    for (int d = 0; d < nd; ++d)
        for (int i = 0; i < p.N; ++i)
            for (int s = 0; s + 1 < 240; ++s) {
                int t = d * 240 + s;
                float c0 = p.close.at(i, t), c1 = p.close.at(i, t + 1);
                if (std::isfinite(c0) && std::isfinite(c1) && c0 != 0.0f)
                    ret.at(i, t) = (c1 / c0 - 1.0f) * 10000.0f;
            }

    std::vector<std::string> exprs, names, errs;
    {
        FILE* f = fopen(factorFile.c_str(), "r");
        if (!f) { fprintf(stderr, "cannot open %s\n", factorFile.c_str()); return 1; }
        char line[4096];
        int idx = 0;
        while (fgets(line, sizeof(line), f)) {
            std::string x(line);
            while (!x.empty() && (x[x.size()-1] == '\n' || x[x.size()-1] == '\r')) x.erase(x.size()-1);
            if (x.empty() || x[0] == '#') continue;
            size_t bar = x.find('|');
            if (bar == std::string::npos) {
                names.push_back("f" + std::to_string(++idx));
                exprs.push_back(x);
            } else {
                names.push_back(x.substr(0, bar));
                exprs.push_back(x.substr(bar + 1));
            }
        }
        fclose(f);
    }
    const int NF = (int)exprs.size();
    fprintf(stderr, "factors=%d\n", NF);

    std::vector<std::vector<Stat> > stats(NF);
    std::vector<float> cov(NF, 0.0f);

    auto t1 = std::chrono::steady_clock::now();
    #pragma omp parallel for schedule(dynamic, 1)
    for (int k = 0; k < NF; ++k) {
        ExprParser ps(exprs[k]);
        ExprNode* root = ps.expr();
        std::string e;
        if (!root) { stats[k].assign(nd, Stat{0,0,0,0,0,0}); continue; }
        bind_windows(root, e);
        if (!e.empty()) { stats[k].assign(nd, Stat{0,0,0,0,0,0}); continue; }

        Mat m;
        try {
            m = eval_expr(root, p);
        } catch (...) {
            stats[k].assign(nd, Stat{0,0,0,0,0,0});
            continue;
        }
        std::vector<Stat> st(nd);
        for (int d = 0; d < nd; ++d) {
            Stat s = {0,0,0,0,0,0};
            for (int i = 0; i < p.N; ++i) {
                for (int sl = 0; sl < 240; ++sl) {
                    int t = d * 240 + sl;
                    float x = m.at(i, t), y = ret.at(i, t);
                    if (std::isfinite(x) && std::isfinite(y)) add_stat(s, x, y);
                }
            }
            st[d] = s;
        }
        stats[k] = st;
        double tot = 0, fin = 0;
        for (int d = 0; d < nd; ++d) {
            for (int i = 0; i < p.N; ++i)
                for (int sl = 0; sl < 240; ++sl) {
                    ++tot;
                    if (std::isfinite(m.at(i, d * 240 + sl))) ++fin;
                }
        }
        cov[k] = (float)(fin / tot);
    }
    auto t2 = std::chrono::steady_clock::now();
    fprintf(stderr, "evaluated %d factors in %.1fs (threads=%d)\n", NF,
            std::chrono::duration<double>(t2 - t1).count(), omp_get_max_threads());

    FILE* fc = fopen(outCsv.c_str(), "w");
    if (!fc) { fprintf(stderr, "cannot write %s\n", outCsv.c_str()); return 1; }
    fprintf(fc, "name,expr,IC_mean,IC_std,IR,pos,IC_pooled,coverage\n");
    for (int k = 0; k < NF; ++k) {
        double sum = 0, sum2 = 0; int cnt = 0, pos = 0;
        Stat tot = {0,0,0,0,0,0};
        for (int d = 0; d < nd; ++d) {
            double ic = stat_ic(stats[k][d]);
            if (std::isfinite(ic)) { sum += ic; sum2 += ic * ic; ++cnt; if (ic > 0) ++pos; }
            const Stat& s = stats[k][d];
            tot.n += s.n; tot.sx += s.sx; tot.sy += s.sy;
            tot.sxy += s.sxy; tot.sxx += s.sxx; tot.syy += s.syy;
        }
        double mean = cnt ? sum / cnt : NAN;
        double var = cnt > 1 ? (sum2 / cnt - mean * mean) : NAN;
        double sd = (var > 0) ? std::sqrt(var) : NAN;
        double ir = (std::isfinite(sd) && sd > 0) ? mean / sd : NAN;
        double pr = cnt ? (double)pos / cnt : NAN;
        double pooled = stat_ic(tot);
        fprintf(fc, "%s,\"%s\",%.6f,%.6f,%.4f,%.4f,%.6f,%.4f\n",
                names[k].c_str(), exprs[k].c_str(), mean, sd, ir, pr, pooled, cov[k]);
    }
    fclose(fc);
    fprintf(stderr, "written %s\n", outCsv.c_str());
    return 0;
}
