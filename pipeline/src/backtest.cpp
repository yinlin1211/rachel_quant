// 因子组合回测：把表达式因子变成「能下单的组合」，输出扣费后的净值与风险指标。
//
// 与 alpha_expr 的区别：alpha_expr 只算 IC（统计量），本工具模拟真实持仓。
//
// 组合层：每分钟取因子最高的 N 只，等权。
// 持有层：把资金分成 H 份（H = 持有分钟数），每份错开 1 分钟进场，持有 H 分钟后退出。
//         这样任意时刻都有 H 个 cohort 在场，且不会出现「同一只股票重复加仓」。
// 执行层：信号用 close[t]，成交用 open[t+1]（下一分钟第一笔中间价），退出用 open[t+1+H]。
//         不跨日：t+1+H 超出当日就放弃该 cohort。
// 成本层：买入 = 滑点 + 佣金；卖出 = 滑点 + 佣金 + 印花税。
//
// 用法：
//   backtest <panel_dir> <factors.txt> <out_prefix> <topN> <hold> <slip_bp> <comm_bp> <stamp_bp> <date1> [date2 ...]
// 输出：
//   <out_prefix>.summary.csv
//   <out_prefix>.nav.csv
#include "expr.h"
#include <omp.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

struct Result {
    double cohorts = 0, trades = 0, sum_net = 0, win = 0;
    double retTot = 0, retAnn = 0, sharpe = 0, maxdd = 0;
    double capMed = 0, capP5 = 0, turnoverDay = 0;
    bool ok = false;
};

// 成交价基准：默认用下一分钟开盘中间价；BT_PRICE=close 时改用下一分钟收盘中间价。
// 用途：验证「开盘价是否粘滞」——若换成收盘价后收益大幅缩水，说明用的是陈旧报价。
static bool g_useClose = false;

static inline float exec_px(const Panel& p, int i, int t) {
    return g_useClose ? p.close.at(i, t) : p.open.at(i, t);
}

static Result run_factor(const Mat& f, const Panel& p, int nd, int topN, int H,
                         double slipBp, double commBp, double stampBp) {
    Result r;
    const double costBuy = (slipBp + commBp) / 1e4;
    const double costSell = (slipBp + commBp + stampBp) / 1e4;
    const double cost = costBuy + costSell;

    std::vector<double> dailyNav;
    std::vector<double> cohortNet;
    std::vector<double> caps;

    std::vector<double> sleeveEnd(H, 1.0), sleevePrev(H, 1.0);
    std::vector<int> idx;
    idx.reserve(p.N);
    std::vector<std::pair<float, int> > cand;
    cand.reserve(p.N);

    // 逐日推进，保证 daily nav 是「同一天内所有 sleeve 的均值」
    for (int d = 0; d < nd; ++d) {
        const int dayStart = d * 240;
        const int dayEnd = dayStart + 239;   // 最后一分钟的下标

        for (int s = 0; s < H; ++s) {
            for (int t = dayStart + s; t + 1 + H <= dayEnd; t += H) {
                cand.clear();
                for (int i = 0; i < p.N; ++i) {
                    float v = f.at(i, t);
                    if (!std::isfinite(v)) continue;
                    float pe = exec_px(p, i, t + 1);
                    float px = exec_px(p, i, t + 1 + H);
                    float am = p.amount.at(i, t);
                    if (!std::isfinite(pe) || !std::isfinite(px) || pe <= 0.0f || px <= 0.0f) continue;
                    if (!std::isfinite(am) || am <= 0.0f) continue;
                    cand.push_back(std::make_pair(v, i));
                }
                if (cand.empty()) continue;
                int k = std::min((int)cand.size(), topN);
                std::partial_sort(cand.begin(), cand.begin() + k, cand.end(),
                                  [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
                                      return a.first > b.first;
                                  });
                double sum = 0.0, cap = 1e30;
                for (int j = 0; j < k; ++j) {
                    int i = cand[j].second;
                    double pe = exec_px(p, i, t + 1), px = exec_px(p, i, t + 1 + H);
                    sum += (px / pe - 1.0);
                    double c = 0.01 * (double)p.amount.at(i, t) * (double)k;  // 每只占该分钟成交额 1%
                    if (c < cap) cap = c;
                }
                double net = sum / k - cost;
                cohortNet.push_back(net);
                caps.push_back(cap);
                r.cohorts += 1;
                r.trades += k;
                r.sum_net += net;
                if (net > 0) r.win += 1;
                sleeveEnd[s] *= (1.0 + net);
            }
        }

        double nav = 0.0;
        for (int s = 0; s < H; ++s) nav += sleeveEnd[s];
        nav /= H;
        dailyNav.push_back(nav);
        for (int s = 0; s < H; ++s) sleevePrev[s] = sleeveEnd[s];
    }

    if (dailyNav.empty() || cohortNet.empty()) return r;
    r.ok = true;

    // 累计收益
    r.retTot = dailyNav.back() - 1.0;

    // 日收益序列
    std::vector<double> dr;
    dr.reserve(dailyNav.size());
    double prev = 1.0;
    for (size_t i = 0; i < dailyNav.size(); ++i) {
        dr.push_back(dailyNav[i] / prev - 1.0);
        prev = dailyNav[i];
    }
    double mu = 0, sd = 0;
    for (size_t i = 0; i < dr.size(); ++i) mu += dr[i];
    mu /= dr.size();
    for (size_t i = 0; i < dr.size(); ++i) sd += (dr[i] - mu) * (dr[i] - mu);
    sd = dr.size() > 1 ? std::sqrt(sd / (dr.size() - 1)) : 0.0;
    r.sharpe = sd > 0 ? mu / sd * std::sqrt(244.0) : 0.0;
    r.retAnn = std::pow(1.0 + r.retTot, 244.0 / (double)dailyNav.size()) - 1.0;

    // 最大回撤
    double peak = dailyNav[0], mdd = 0;
    for (size_t i = 0; i < dailyNav.size(); ++i) {
        if (dailyNav[i] > peak) peak = dailyNav[i];
        double dd = 1.0 - dailyNav[i] / peak;
        if (dd > mdd) mdd = dd;
    }
    r.maxdd = mdd;

    // 容量：取 5 分位与中位数
    std::sort(caps.begin(), caps.end());
    r.capP5 = caps[(size_t)(0.05 * (caps.size() - 1))];
    r.capMed = caps[caps.size() / 2];

    // 换手：每个 sleeve 每天 240/H 次往返，资金量 = 2 * 240/H
    r.turnoverDay = 2.0 * 240.0 / (double)H;
    return r;
}

int main(int argc, char** argv) {
    if (argc < 10) {
        fprintf(stderr,
                "usage: %s <panel_dir> <factors.txt> <out_prefix> <topN> <hold> "
                "<slip_bp> <comm_bp> <stamp_bp> <date1> [date2 ...]\n",
                argv[0]);
        return 1;
    }
    std::string panelDir = argv[1];
    std::string factorFile = argv[2];
    std::string outPrefix = argv[3];
    int topN = atoi(argv[4]);
    int H = atoi(argv[5]);
    double slipBp = atof(argv[6]);
    double commBp = atof(argv[7]);
    double stampBp = atof(argv[8]);
    std::vector<int> dates;
    for (int i = 9; i < argc; ++i) dates.push_back(atoi(argv[i]));
    const int nd = (int)dates.size();
    if (H < 1 || topN < 1) { fprintf(stderr, "bad topN/hold\n"); return 1; }

    const char* pm = getenv("BT_PRICE");
    if (pm && std::string(pm) == "close") g_useClose = true;

    Panel p;
    if (!load_panel(panelDir, dates, p)) return 1;
    fprintf(stderr, "panel T=%d N=%d days=%d topN=%d hold=%d cost=%.1f/%.1f/%.1f bp\n",
            p.T, p.N, nd, topN, H, slipBp, commBp, stampBp);

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
    if (NF == 0) { fprintf(stderr, "no factors\n"); return 1; }

    std::vector<Result> res(NF);

    auto t1 = std::chrono::steady_clock::now();
    #pragma omp parallel for schedule(dynamic, 1)
    for (int k = 0; k < NF; ++k) {
        ExprParser ps(exprs[k]);
        ExprNode* root = ps.expr();
        std::string e;
        if (!root) { fprintf(stderr, "FACTOR %s: parse error\n", names[k].c_str()); continue; }
        bind_windows(root, e);
        if (!e.empty()) { fprintf(stderr, "FACTOR %s: %s\n", names[k].c_str(), e.c_str()); continue; }
        Mat m = eval_expr(root, p);
        res[k] = run_factor(m, p, nd, topN, H, slipBp, commBp, stampBp);
    }
    auto t2 = std::chrono::steady_clock::now();
    fprintf(stderr, "backtested %d factors in %.1fs\n", NF,
            std::chrono::duration<double>(t2 - t1).count());

    char path[1024];
    snprintf(path, sizeof(path), "%s.summary.csv", outPrefix.c_str());
    FILE* fc = fopen(path, "w");
    if (!fc) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    fprintf(fc, "name,expr,cohorts,trades_per_day,avg_net_bp,win_rate,ret_total,ret_ann,"
                "sharpe,maxdd,turnover_day,cap_median,cap_p5\n");
    for (int k = 0; k < NF; ++k) {
        const Result& r = res[k];
        fprintf(fc, "%s,\"%s\",%.0f,%.1f,%.2f,%.3f,%.4f,%.4f,%.3f,%.4f,%.1f,%.0f,%.0f\n",
                names[k].c_str(), exprs[k].c_str(),
                r.cohorts,
                nd > 0 ? r.trades / nd : 0.0,
                r.cohorts > 0 ? r.sum_net / r.cohorts * 1e4 : 0.0,
                r.cohorts > 0 ? r.win / r.cohorts : 0.0,
                r.retTot, r.retAnn, r.sharpe, r.maxdd, r.turnoverDay, r.capMed, r.capP5);
    }
    fclose(fc);
    printf("written %s\n", path);
    return 0;
}
