// 表达式因子批量求值：读 factors.txt，逐个求值并统计 IC（不回写因子值）
//
// 评估内容：
//   - 三个预测周期：未来 1 / 3 / 5 分钟收益
//   - 逐日 IC（用于看稳定性）、IC 均值/标准差/IR/同向占比、合并 IC
//   - 覆盖率
// 输出：
//   <out.csv>        汇总
//   <out>.daily.csv  逐日 IC（1 分钟周期）
#include "expr.h"
#include <omp.h>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

static const int NHZ = 3;
static const int HZ[NHZ] = {1, 3, 5};

struct Stat { double n, sx, sy, sxy, sxx, syy; };

static inline void add_stat(Stat& s, double x, double y) {
    s.n += 1; s.sx += x; s.sy += y;
    s.sxy += x * y; s.sxx += x * x; s.syy += y * y;
}

static inline void add_stat_to(Stat& s, const Stat& o) {
    s.n += o.n; s.sx += o.sx; s.sy += o.sy;
    s.sxy += o.sxy; s.sxx += o.sxx; s.syy += o.syy;
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
    const int nd = (int)dates.size();

    Panel p;
    auto t0 = std::chrono::steady_clock::now();
    if (!load_panel(panelDir, dates, p)) return 1;
    fprintf(stderr, "panel T=%d N=%d days=%d\n", p.T, p.N, nd);

    // 未来 h 分钟收益（基点），按天截断，不跨日
    std::vector<Mat> rets(NHZ);
    for (int h = 0; h < NHZ; ++h) {
        rets[h] = Mat(p.T, p.N);
        const int n = HZ[h];
        for (int d = 0; d < nd; ++d)
            for (int i = 0; i < p.N; ++i)
                for (int s = 0; s + n < 240; ++s) {
                    int t = d * 240 + s;
                    float c0 = p.close.at(i, t), c1 = p.close.at(i, t + n);
                    if (std::isfinite(c0) && std::isfinite(c1) && c0 != 0.0f)
                        rets[h].at(i, t) = (c1 / c0 - 1.0f) * 10000.0f;
                }
    }

    std::vector<std::string> exprs, names;
    {
        FILE* f = fopen(factorFile.c_str(), "r");
        if (!f) { fprintf(stderr, "cannot open %s\n", factorFile.c_str()); return 1; }
        char line[8192];
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

    // stats[k * nd * NHZ + d * NHZ + h]
    std::vector<Stat> stats((size_t)NF * nd * NHZ);
    for (size_t z = 0; z < stats.size(); ++z) stats[z] = Stat{0,0,0,0,0,0};
    std::vector<float> cov(NF, 0.0f);

    auto t1 = std::chrono::steady_clock::now();
    #pragma omp parallel for schedule(dynamic, 1)
    for (int k = 0; k < NF; ++k) {
        ExprParser ps(exprs[k]);
        ExprNode* root = ps.expr();
        std::string e;
        if (!root) continue;
        bind_windows(root, e);
        if (!e.empty()) continue;

        Mat m = eval_expr(root, p);

        size_t base = (size_t)k * nd * NHZ;
        double tot = 0, fin = 0;
        for (int d = 0; d < nd; ++d) {
            Stat st[NHZ];
            for (int h = 0; h < NHZ; ++h) st[h] = Stat{0,0,0,0,0,0};
            for (int i = 0; i < p.N; ++i) {
                for (int s = 0; s < 240; ++s) {
                    int t = d * 240 + s;
                    float x = m.at(i, t);
                    ++tot;
                    if (!std::isfinite(x)) continue;
                    ++fin;
                    for (int h = 0; h < NHZ; ++h) {
                        float y = rets[h].at(i, t);
                        if (std::isfinite(y)) add_stat(st[h], x, y);
                    }
                }
            }
            for (int h = 0; h < NHZ; ++h) stats[base + (size_t)d * NHZ + h] = st[h];
        }
        cov[k] = (float)(fin / tot);
    }
    auto t2 = std::chrono::steady_clock::now();
    fprintf(stderr, "evaluated %d factors in %.1fs (threads=%d)\n", NF,
            std::chrono::duration<double>(t2 - t1).count(), omp_get_max_threads());

    // ---------- 汇总 ----------
    FILE* fc = fopen(outCsv.c_str(), "w");
    if (!fc) { fprintf(stderr, "cannot write %s\n", outCsv.c_str()); return 1; }
    fprintf(fc, "name,expr");
    for (int h = 0; h < NHZ; ++h)
        fprintf(fc, ",IC%d_mean,IC%d_std,IR%d,pos%d,IC%d_pooled", HZ[h], HZ[h], HZ[h], HZ[h], HZ[h]);
    fprintf(fc, ",coverage\n");

    for (int k = 0; k < NF; ++k) {
        fprintf(fc, "%s,\"%s\"", names[k].c_str(), exprs[k].c_str());
        size_t base = (size_t)k * nd * NHZ;
        for (int h = 0; h < NHZ; ++h) {
            double sum = 0, sum2 = 0;
            int cnt = 0, pos = 0;
            Stat tot = {0,0,0,0,0,0};
            for (int d = 0; d < nd; ++d) {
                const Stat& s = stats[base + (size_t)d * NHZ + h];
                double ic = stat_ic(s);
                if (std::isfinite(ic)) { sum += ic; sum2 += ic * ic; ++cnt; if (ic > 0) ++pos; }
                add_stat_to(tot, s);
            }
            double mean = cnt ? sum / cnt : NAN;
            double var = cnt > 1 ? (sum2 / cnt - mean * mean) : NAN;
            double sd = (var > 0) ? std::sqrt(var) : NAN;
            double ir = (std::isfinite(sd) && sd > 0) ? mean / sd : NAN;
            double pr = cnt ? (double)pos / cnt : NAN;
            double pooled = stat_ic(tot);
            fprintf(fc, ",%.6f,%.6f,%.4f,%.4f,%.6f", mean, sd, ir, pr, pooled);
        }
        fprintf(fc, ",%.4f\n", cov[k]);
    }
    fclose(fc);

    // ---------- 逐日 IC（1 分钟周期） ----------
    {
        std::string daily = outCsv + ".daily.csv";
        FILE* fd = fopen(daily.c_str(), "w");
        if (fd) {
            fprintf(fd, "name");
            for (int d = 0; d < nd; ++d) fprintf(fd, ",%d", dates[d]);
            fprintf(fd, "\n");
            for (int k = 0; k < NF; ++k) {
                fprintf(fd, "%s", names[k].c_str());
                for (int d = 0; d < nd; ++d)
                    fprintf(fd, ",%.6f", stat_ic(stats[(size_t)k * nd * NHZ + (size_t)d * NHZ + 0]));
                fprintf(fd, "\n");
            }
            fclose(fd);
        }
    }

    fprintf(stderr, "written %s (+ .daily.csv)\n", outCsv.c_str());
    return 0;
}
