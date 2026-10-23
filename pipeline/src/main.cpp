// Alpha101 主程序：加载多日分钟面板 -> 计算 82 条因子 -> 按日写出 .mkt/.dat
#include "alphas.cpp"
#include "panel.h"
#include <omp.h>
#include <chrono>
#include <sys/stat.h>
#include <cerrno>

static const int NALPHAS = (int)(sizeof(ALPHAS) / sizeof(ALPHAS[0]));

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <panel_dir> <out_dir> <date1> [date2 ...]\n", argv[0]);
        return 1;
    }
    std::string panelDir = argv[1];
    std::string outDir = argv[2];
    std::vector<int> dates;
    for (int i = 3; i < argc; ++i) dates.push_back(atoi(argv[i]));

    struct stat st;
    if (stat(outDir.c_str(), &st) != 0) mkdir(outDir.c_str(), 0755);

    Panel p;
    auto t0 = std::chrono::steady_clock::now();
    if (!load_panel(panelDir, dates, p)) return 1;
    auto t1 = std::chrono::steady_clock::now();
    fprintf(stderr, "panel loaded T=%d N=%d %.1fs\n", p.T, p.N,
            std::chrono::duration<double>(t1 - t0).count());

    Mat returns = subc(divv(p.close, delay(p.close, 1)), 1.0f); // close/delay(close,1)-1

    AllData data;
    data.open = &p.open; data.high = &p.high; data.low = &p.low; data.close = &p.close;
    data.vwap = &p.vwap; data.volume = &p.volume; data.amount = &p.amount;
    data.returns = &returns; data.T = p.T; data.N = p.N;

    std::vector<Mat> results(NALPHAS);
    auto t2 = std::chrono::steady_clock::now();
    #pragma omp parallel for schedule(dynamic, 1)
    for (int k = 0; k < NALPHAS; ++k) {
        results[k] = ALPHAS[k].fn(data);
    }
    auto t3 = std::chrono::steady_clock::now();
    fprintf(stderr, "alpha101 computed %d factors, threads=%d, %.1fs\n", NALPHAS,
            omp_get_max_threads(), std::chrono::duration<double>(t3 - t2).count());

    // ---------- 在内存里直接算 IC 的充分统计量（不落盘、不回读） ----------
    // 每个 (日期, 因子, 预测周期) 只需 n, Σx, Σy, Σxy, Σx², Σy²，
    // 之后由 Python 用它们组合出逐日 IC 与合并 IC。
    static const int HZ[] = {1, 3, 5};
    const int NHZ = (int)(sizeof(HZ) / sizeof(HZ[0]));
    const int nd = (int)dates.size();

    std::vector<Mat> rets(NHZ);
    for (int h = 0; h < NHZ; ++h) {
        rets[h] = Mat(p.T, p.N);
        const int n = HZ[h];
        for (int d = 0; d < nd; ++d) {
            for (int i = 0; i < p.N; ++i) {
                for (int s = 0; s + n < 240; ++s) {
                    int t = d * 240 + s;
                    float c0 = p.close.at(i, t), c1 = p.close.at(i, t + n);
                    if (std::isfinite(c0) && std::isfinite(c1) && c0 != 0.0f)
                        rets[h].at(i, t) = (c1 / c0 - 1.0f) * 10000.0f;
                }
            }
        }
    }

    struct Acc { double n, sx, sy, sxy, sxx, syy; };
    std::vector<Acc> acc((size_t)NALPHAS * NHZ * nd);
    for (size_t z = 0; z < acc.size(); ++z) {
        acc[z].n = acc[z].sx = acc[z].sy = acc[z].sxy = acc[z].sxx = acc[z].syy = 0.0;
    }

    auto t3b = std::chrono::steady_clock::now();
    #pragma omp parallel for schedule(dynamic, 1)
    for (int k = 0; k < NALPHAS; ++k) {
        for (int d = 0; d < nd; ++d) {
            Acc a[8];
            for (int h = 0; h < NHZ; ++h) a[h].n = a[h].sx = a[h].sy = a[h].sxy = a[h].sxx = a[h].syy = 0.0;
            for (int i = 0; i < p.N; ++i) {
                for (int s = 0; s < 240; ++s) {
                    int t = d * 240 + s;
                    float x = results[k].at(i, t);
                    if (!std::isfinite(x)) continue;
                    for (int h = 0; h < NHZ; ++h) {
                        float y = rets[h].at(i, t);
                        if (!std::isfinite(y)) continue;
                        Acc& q = a[h];
                        q.n += 1; q.sx += x; q.sy += y;
                        q.sxy += (double)x * y; q.sxx += (double)x * x; q.syy += (double)y * y;
                    }
                }
            }
            for (int h = 0; h < NHZ; ++h) {
                acc[((size_t)k * NHZ + h) * nd + d] = a[h];
            }
        }
    }
    auto t3c = std::chrono::steady_clock::now();
    fprintf(stderr, "ic stats %.1fs\n", std::chrono::duration<double>(t3c - t3b).count());

    // ---------- 因子相关性矩阵（82×82），用于去重 ----------
    // 先把每个因子在全体有效值上标准化，NaN 填 0（占比约 2%，对去重判断无影响），
    // 再累加 Gram 矩阵。按 (股票, 日) 分块，块内 82×240 个浮点数可放进 L2。
    std::vector<double> mu(NALPHAS, 0.0), invsd(NALPHAS, 0.0);
    #pragma omp parallel for schedule(static)
    for (int k = 0; k < NALPHAS; ++k) {
        double s = 0.0, s2 = 0.0;
        long long n = 0;
        const std::vector<float>& v = results[k].d;
        for (size_t z = 0; z < v.size(); ++z) {
            float x = v[z];
            if (std::isfinite(x)) { s += x; s2 += (double)x * x; ++n; }
        }
        if (n > 1) {
            mu[k] = s / (double)n;
            double var = s2 / (double)n - mu[k] * mu[k];
            invsd[k] = (var > 0.0) ? 1.0 / std::sqrt(var) : 0.0;
        }
    }

    auto t5 = std::chrono::steady_clock::now();
    std::vector<double> gram((size_t)NALPHAS * NALPHAS, 0.0);
    #pragma omp parallel
    {
        std::vector<double> g((size_t)NALPHAS * NALPHAS, 0.0);
        std::vector<float> blk((size_t)NALPHAS * 240);
        #pragma omp for schedule(dynamic, 16)
        for (int i = 0; i < p.N; ++i) {
            for (int d = 0; d < nd; ++d) {
                const int t0 = d * 240;
                for (int k = 0; k < NALPHAS; ++k) {
                    const float* src = &results[k].d[(size_t)i * p.T + t0];
                    float* dst = &blk[(size_t)k * 240];
                    for (int s = 0; s < 240; ++s) {
                        float v = src[s];
                        dst[s] = std::isfinite(v) ? (float)((v - mu[k]) * invsd[k]) : 0.0f;
                    }
                }
                for (int s = 0; s < 240; ++s) {
                    for (int a = 0; a < NALPHAS; ++a) {
                        float va = blk[(size_t)a * 240 + s];
                        if (va == 0.0f) continue;
                        double* ga = &g[(size_t)a * NALPHAS];
                        const float* base = &blk[s];
                        for (int b = a; b < NALPHAS; ++b)
                            ga[b] += (double)va * base[(size_t)b * 240];
                    }
                }
            }
        }
        #pragma omp critical
        for (size_t z = 0; z < gram.size(); ++z) gram[z] += g[z];
    }
    auto t6 = std::chrono::steady_clock::now();
    fprintf(stderr, "corr matrix %.1fs\n", std::chrono::duration<double>(t6 - t5).count());

    {
        char cp[1024];
        snprintf(cp, sizeof(cp), "%s/corr_matrix.csv", outDir.c_str());
        FILE* fc = fopen(cp, "w");
        if (fc) {
            fprintf(fc, "name");
            for (int k = 0; k < NALPHAS; ++k) fprintf(fc, ",%s", ALPHAS[k].name);
            fprintf(fc, "\n");
            for (int a = 0; a < NALPHAS; ++a) {
                fprintf(fc, "%s", ALPHAS[a].name);
                for (int b = 0; b < NALPHAS; ++b) {
                    double va = gram[(size_t)a * NALPHAS + a];
                    double vb = gram[(size_t)b * NALPHAS + b];
                    // gram 只累加了上三角，取对称位置
                    size_t idx = (a <= b) ? (size_t)a * NALPHAS + b : (size_t)b * NALPHAS + a;
                    double c = (va > 0 && vb > 0) ? gram[idx] / std::sqrt(va * vb) : 0.0;
                    fprintf(fc, ",%.4f", c);
                }
                fprintf(fc, "\n");
            }
            fclose(fc);
        }
    }

    {
        char icp[1024];
        snprintf(icp, sizeof(icp), "%s/ic_stats.csv", outDir.c_str());
        FILE* fic = fopen(icp, "w");
        if (!fic) { fprintf(stderr, "cannot write %s\n", icp); return 1; }
        fprintf(fic, "date,factor,horizon,n,sx,sy,sxy,sxx,syy\n");
        for (int k = 0; k < NALPHAS; ++k)
            for (int h = 0; h < NHZ; ++h)
                for (int d = 0; d < nd; ++d) {
                    const Acc& q = acc[((size_t)k * NHZ + h) * nd + d];
                    fprintf(fic, "%d,%s,%d,%.0f,%.17g,%.17g,%.17g,%.17g,%.17g\n",
                            dates[d], ALPHAS[k].name, HZ[h], q.n, q.sx, q.sy, q.sxy, q.sxx, q.syy);
                }
        fclose(fic);
    }

    // alphaInfo
    char info[1024];
    snprintf(info, sizeof(info), "%s/alphaInfo.txt", outDir.c_str());
    FILE* fi = fopen(info, "w");
    if (!fi) { fprintf(stderr, "cannot write %s\n", info); return 1; }
    fprintf(fi, "%d,%d\n", p.N, NALPHAS);
    for (int k = 0; k < NALPHAS; ++k) fprintf(fi, "%s\n", ALPHAS[k].name);
    fclose(fi);

    // 股票代码
    FILE* ft = fopen("/opt/bar_data/Meta/tkr.txt", "r");
    std::vector<int> tkrs;
    if (ft) {
        char line[128];
        while (fgets(line, sizeof(line), ft)) tkrs.push_back(atoi(line));
        fclose(ft);
    }
    if ((int)tkrs.size() != p.N) { fprintf(stderr, "tkr count mismatch %zu vs %d\n", tkrs.size(), p.N); return 1; }

    // 按天并行写：每个线程独占一天的缓冲区，各天之间无共享
    #pragma omp parallel for schedule(dynamic, 1)
    for (int dd = 0; dd < (int)dates.size(); ++dd) {
        const size_t d = (size_t)dd;
        std::vector<float> datBuf((size_t)p.N * NALPHAS);
        std::vector<int32_t> mktBuf((size_t)p.N * 4);
        char mp[1024], dp[1024];
        snprintf(mp, sizeof(mp), "%s/%d.mkt", outDir.c_str(), dates[d]);
        snprintf(dp, sizeof(dp), "%s/%d.dat", outDir.c_str(), dates[d]);
        FILE* fm = fopen(mp, "wb");
        FILE* fa = fopen(dp, "wb");
        if (!fm || !fa) {
            fprintf(stderr, "cannot open output for %d\n", dates[d]);
            if (fm) fclose(fm);
            if (fa) fclose(fa);
            continue;
        }
        for (int s = 0; s < 240; ++s) {
            int t = (int)d * 240 + s;
            int minute = (s < 120) ? (570 + s) : (780 + s - 120);
            int hhmmss = minute / 60 * 10000 + minute % 60 * 100;
            for (int i = 0; i < p.N; ++i) {
                float cl = p.close.at(i, t);
                mktBuf[(size_t)i * 4 + 0] = hhmmss * 1000;
                mktBuf[(size_t)i * 4 + 1] = hhmmss;
                mktBuf[(size_t)i * 4 + 2] = tkrs[i];
                mktBuf[(size_t)i * 4 + 3] = std::isfinite(cl) ? (int32_t)cl : 0;
                for (int k = 0; k < NALPHAS; ++k)
                    datBuf[(size_t)i * NALPHAS + k] = results[k].at(i, t);
            }
            fwrite(&mktBuf[0], sizeof(int32_t), (size_t)p.N * 4, fm);
            fwrite(&datBuf[0], sizeof(float), (size_t)p.N * NALPHAS, fa);
        }
        fclose(fm);
        fclose(fa);
    }
    auto t4 = std::chrono::steady_clock::now();
    fprintf(stderr, "written %.1fs total=%.1fs\n",
            std::chrono::duration<double>(t4 - t3).count(),
            std::chrono::duration<double>(t4 - t0).count());
    return 0;
}
