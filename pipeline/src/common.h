// Alpha101 计算核心：矩阵类型、时序算子、截面算子
// 内存布局统一为 stock-major: d[i*T + t]，时序算子按股票连续访问，截面算子按时间 gather。
#ifndef A101_COMMON_H
#define A101_COMMON_H

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
#include <vector>
#include <string>
#include <algorithm>
#include <deque>
#include <utility>

static const float F_NAN = std::numeric_limits<float>::quiet_NaN();

#pragma pack(push, 1)
struct PanelRow {
    int32_t nTime, minute, cidx, nMd, nTs;
    float open, high, low, close, vwap, volume, amount;
};
#pragma pack(pop)

struct Mat {
    int T, N;
    std::vector<float> d;
    Mat() : T(0), N(0) {}
    Mat(int t, int n) : T(t), N(n), d((size_t)t * (size_t)n, F_NAN) {}
    inline float&  at(int i, int t)       { return d[(size_t)i * T + t]; }
    inline float   at(int i, int t) const { return d[(size_t)i * T + t]; }
};

struct Mask {
    int T, N;
    std::vector<unsigned char> d;
    Mask(int t, int n) : T(t), N(n), d((size_t)t * (size_t)n, 0) {}
    inline unsigned char&  at(int i, int t)       { return d[(size_t)i * T + t]; }
    inline unsigned char   at(int i, int t) const { return d[(size_t)i * T + t]; }
};

// ---------------- 逐元素 ----------------
inline Mat add(const Mat& a, const Mat& b) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = a.d[k] + b.d[k];
    return r;
}
inline Mat sub(const Mat& a, const Mat& b) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = a.d[k] - b.d[k];
    return r;
}
inline Mat mul(const Mat& a, const Mat& b) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = a.d[k] * b.d[k];
    return r;
}
inline Mat divv(const Mat& a, const Mat& b) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) {
        float y = b.d[k];
        r.d[k] = (y == 0.0f) ? F_NAN : a.d[k] / y;
    }
    return r;
}
inline Mat addc(const Mat& a, float c) { Mat r(a.T, a.N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = a.d[k] + c; return r; }
inline Mat subc(const Mat& a, float c) { Mat r(a.T, a.N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = a.d[k] - c; return r; }
inline Mat rsubc(float c, const Mat& a) { Mat r(a.T, a.N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = c - a.d[k]; return r; }
inline Mat mulc(const Mat& a, float c) { Mat r(a.T, a.N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = a.d[k] * c; return r; }
inline Mat divc(const Mat& a, float c) { Mat r(a.T, a.N); if (c == 0.0f) return r; for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = a.d[k] / c; return r; }
inline Mat rdivc(float c, const Mat& a) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = (a.d[k] == 0.0f) ? F_NAN : c / a.d[k];
    return r;
}
inline Mat negv(const Mat& a) { return mulc(a, -1.0f); }
inline Mat absv(const Mat& a) { Mat r(a.T, a.N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = std::fabs(a.d[k]); return r; }
inline Mat logv(const Mat& a) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) { float v = a.d[k]; r.d[k] = (v > 0.0f) ? std::log(v) : F_NAN; }
    return r;
}
inline Mat sqrtv(const Mat& a) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) { float v = a.d[k]; r.d[k] = (v >= 0.0f) ? std::sqrt(v) : F_NAN; }
    return r;
}
inline Mat signv(const Mat& a) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) { float v = a.d[k]; r.d[k] = (v > 0.0f) ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); if (!std::isfinite(v)) r.d[k] = F_NAN; }
    return r;
}
inline Mat powv(const Mat& a, const Mat& b) { Mat r(a.T, a.N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = std::pow(a.d[k], b.d[k]); return r; }
inline Mat powc(const Mat& a, float c) { Mat r(a.T, a.N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = std::pow(a.d[k], c); return r; }
// SignedPower(x, y) = sign(x) * |x|^y
inline Mat spow(const Mat& a, const Mat& b) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) {
        float x = a.d[k], y = b.d[k];
        if (!std::isfinite(x) || !std::isfinite(y)) { r.d[k] = F_NAN; continue; }
        r.d[k] = (x >= 0.0f ? 1.0f : -1.0f) * std::pow(std::fabs(x), y);
    }
    return r;
}
inline Mat spowc(const Mat& a, float c) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) {
        float x = a.d[k];
        if (!std::isfinite(x)) { r.d[k] = F_NAN; continue; }
        r.d[k] = (x >= 0.0f ? 1.0f : -1.0f) * std::pow(std::fabs(x), c);
    }
    return r;
}
inline Mat max2(const Mat& a, const Mat& b) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) {
        float x = a.d[k], y = b.d[k];
        if (std::isnan(x)) r.d[k] = y;
        else if (std::isnan(y)) r.d[k] = x;
        else r.d[k] = x > y ? x : y;
    }
    return r;
}
inline Mat min2(const Mat& a, const Mat& b) {
    Mat r(a.T, a.N);
    for (size_t k = 0; k < r.d.size(); ++k) {
        float x = a.d[k], y = b.d[k];
        if (std::isnan(x)) r.d[k] = y;
        else if (std::isnan(y)) r.d[k] = x;
        else r.d[k] = x < y ? x : y;
    }
    return r;
}

// ---------------- 比较 ----------------
inline Mask lt(const Mat& a, const Mat& b) { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = (a.d[k] < b.d[k]) ? 1 : 0; return m; }
inline Mask gt(const Mat& a, const Mat& b) { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = (a.d[k] > b.d[k]) ? 1 : 0; return m; }
inline Mask ltc(const Mat& a, float c) { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = (a.d[k] < c) ? 1 : 0; return m; }
inline Mask gtc(const Mat& a, float c) { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = (a.d[k] > c) ? 1 : 0; return m; }
inline Mask lec(const Mat& a, float c) { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = (a.d[k] <= c) ? 1 : 0; return m; }
inline Mask gec(const Mat& a, float c) { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = (a.d[k] >= c) ? 1 : 0; return m; }
inline Mask m_and(const Mask& a, const Mask& b) { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = (a.d[k] && b.d[k]) ? 1 : 0; return m; }
inline Mask m_or(const Mask& a, const Mask& b)  { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = (a.d[k] || b.d[k]) ? 1 : 0; return m; }
inline Mask m_not(const Mask& a) { Mask m(a.T, a.N); for (size_t k = 0; k < m.d.size(); ++k) m.d[k] = a.d[k] ? 0 : 1; return m; }

inline Mat select(const Mask& m, const Mat& a, const Mat& b) {
    Mat r(m.T, m.N);
    for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = m.d[k] ? a.d[k] : b.d[k];
    return r;
}
inline Mat selectc(const Mask& m, float a, float b) {
    Mat r(m.T, m.N);
    for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = m.d[k] ? a : b;
    return r;
}
// bool -> 1.0 / 0.0
inline Mat bool10(const Mask& m) { return selectc(m, 1.0f, 0.0f); }

// ---------------- 截面 ----------------
// 每个时间截面上对有效值做排名，归一化到 [0,1]（并列取平均名次），NaN 保持 NaN
inline Mat rankv(const Mat& x) {
    Mat r(x.T, x.N);
    std::vector<std::pair<float, int> > v;
    v.reserve(x.N);
    for (int t = 0; t < x.T; ++t) {
        v.clear();
        for (int i = 0; i < x.N; ++i) {
            float u = x.at(i, t);
            if (std::isfinite(u)) v.push_back(std::make_pair(u, i));
        }
        int n = (int)v.size();
        if (n == 0) continue;
        std::sort(v.begin(), v.end());
        int k = 0;
        while (k < n) {
            int k2 = k;
            while (k2 + 1 < n && v[k2 + 1].first == v[k].first) ++k2;
            float rk = (n == 1) ? 0.5f : ((float)(k + k2) * 0.5f / (float)(n - 1));
            for (int q = k; q <= k2; ++q) r.at(v[q].second, t) = rk;
            k = k2 + 1;
        }
    }
    return r;
}

inline Mat scalev(const Mat& x, float a = 1.0f) {
    Mat r(x.T, x.N);
    for (int t = 0; t < x.T; ++t) {
        double s = 0.0;
        int cnt = 0;
        for (int i = 0; i < x.N; ++i) {
            float u = x.at(i, t);
            if (std::isfinite(u)) { s += std::fabs((double)u); ++cnt; }
        }
        if (cnt == 0) continue;
        if (s == 0.0) { for (int i = 0; i < x.N; ++i) if (std::isfinite(x.at(i, t))) r.at(i, t) = 0.0f; continue; }
        double inv = (double)a / s;
        for (int i = 0; i < x.N; ++i) {
            float u = x.at(i, t);
            if (std::isfinite(u)) r.at(i, t) = (float)(u * inv);
        }
    }
    return r;
}

// ---------------- 时序算子 ----------------
inline int wr(int w) { return w < 1 ? 1 : w; }

inline Mat ts_sum(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    std::vector<float> buf(w, F_NAN);
    for (int i = 0; i < x.N; ++i) {
        double s = 0.0;
        int nanCnt = 0;
        for (int t = 0; t < x.T; ++t) {
            int slot = t % w;
            if (t >= w) {
                float old = buf[slot];
                if (std::isfinite(old)) s -= old; else --nanCnt;
            }
            float v = x.at(i, t);
            buf[slot] = v;
            if (std::isfinite(v)) s += v; else ++nanCnt;
            if (t >= w - 1 && nanCnt == 0) r.at(i, t) = (float)s;
        }
        for (int k = 0; k < w; ++k) buf[k] = F_NAN;
    }
    return r;
}

inline Mat ts_mean(const Mat& x, int w) { return divc(ts_sum(x, w), (float)wr(w)); }

inline Mat ts_std(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    std::vector<float> buf(w, F_NAN);
    for (int i = 0; i < x.N; ++i) {
        double s = 0.0, s2 = 0.0;
        int nanCnt = 0;
        for (int t = 0; t < x.T; ++t) {
            int slot = t % w;
            if (t >= w) {
                float old = buf[slot];
                if (std::isfinite(old)) { s -= old; s2 -= (double)old * old; } else --nanCnt;
            }
            float v = x.at(i, t);
            buf[slot] = v;
            if (std::isfinite(v)) { s += v; s2 += (double)v * v; } else ++nanCnt;
            if (t >= w - 1 && nanCnt == 0) {
                double m = s / w;
                double var = s2 / w - m * m;
                if (var < 0.0) var = 0.0;
                r.at(i, t) = (float)std::sqrt(var);
            }
        }
        for (int k = 0; k < w; ++k) buf[k] = F_NAN;
    }
    return r;
}

inline Mat ts_corr(const Mat& x, const Mat& y, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    std::vector<float> bx(w, F_NAN), by(w, F_NAN);
    for (int i = 0; i < x.N; ++i) {
        double sx = 0, sy = 0, sxy = 0, sxx = 0, syy = 0;
        int bad = 0;
        for (int t = 0; t < x.T; ++t) {
            int slot = t % w;
            if (t >= w) {
                float ox = bx[slot], oy = by[slot];
                if (std::isfinite(ox) && std::isfinite(oy)) {
                    sx -= ox; sy -= oy; sxy -= (double)ox * oy; sxx -= (double)ox * ox; syy -= (double)oy * oy;
                } else --bad;   // 一个 NaN 离开窗口，坏值计数要减少
            }
            float vx = x.at(i, t), vy = y.at(i, t);
            bx[slot] = vx; by[slot] = vy;
            if (std::isfinite(vx) && std::isfinite(vy)) {
                sx += vx; sy += vy; sxy += (double)vx * vy; sxx += (double)vx * vx; syy += (double)vy * vy;
            } else ++bad;
            if (t >= w - 1 && bad == 0) {
                double n = w;
                double cov = n * sxy - sx * sy;
                double vx2 = n * sxx - sx * sx;
                double vy2 = n * syy - sy * sy;
                double den = std::sqrt(vx2 * vy2);
                r.at(i, t) = (den > 0.0) ? (float)(cov / den) : F_NAN;
            }
        }
        for (int k = 0; k < w; ++k) { bx[k] = F_NAN; by[k] = F_NAN; }
    }
    return r;
}

inline Mat ts_cov(const Mat& x, const Mat& y, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    std::vector<float> bx(w, F_NAN), by(w, F_NAN);
    for (int i = 0; i < x.N; ++i) {
        double sx = 0, sy = 0, sxy = 0;
        int bad = 0;
        for (int t = 0; t < x.T; ++t) {
            int slot = t % w;
            if (t >= w) {
                float ox = bx[slot], oy = by[slot];
                if (std::isfinite(ox) && std::isfinite(oy)) { sx -= ox; sy -= oy; sxy -= (double)ox * oy; }
                else --bad;   // 一个 NaN 离开窗口，坏值计数要减少
            }
            float vx = x.at(i, t), vy = y.at(i, t);
            bx[slot] = vx; by[slot] = vy;
            if (std::isfinite(vx) && std::isfinite(vy)) { sx += vx; sy += vy; sxy += (double)vx * vy; }
            else ++bad;
            if (t >= w - 1 && bad == 0) r.at(i, t) = (float)(sxy / w - (sx / w) * (sy / w));
        }
        for (int k = 0; k < w; ++k) { bx[k] = F_NAN; by[k] = F_NAN; }
    }
    return r;
}

// ts_max / ts_min: 单调队列 + 窗口内 NaN 计数
inline Mat ts_max(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    std::deque<int> dq;
    std::vector<unsigned char> bad(w, 0);
    for (int i = 0; i < x.N; ++i) {
        dq.clear();
        int nanCnt = 0;
        for (int t = 0; t < x.T; ++t) {
            int slot = t % w;
            if (t >= w && bad[slot]) --nanCnt;
            float v = x.at(i, t);
            bad[slot] = std::isfinite(v) ? 0 : 1;
            if (bad[slot]) ++nanCnt;
            while (!dq.empty() && dq.back() <= t - w) dq.pop_back();
            if (std::isfinite(v)) {
                while (!dq.empty() && x.at(i, dq.front()) <= v) dq.pop_front();
                dq.push_front(t);
            }
            if (t >= w - 1 && nanCnt == 0 && !dq.empty()) r.at(i, t) = x.at(i, dq.back());
        }
    }
    return r;
}

inline Mat ts_min(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    std::deque<int> dq;
    std::vector<unsigned char> bad(w, 0);
    for (int i = 0; i < x.N; ++i) {
        dq.clear();
        int nanCnt = 0;
        for (int t = 0; t < x.T; ++t) {
            int slot = t % w;
            if (t >= w && bad[slot]) --nanCnt;
            float v = x.at(i, t);
            bad[slot] = std::isfinite(v) ? 0 : 1;
            if (bad[slot]) ++nanCnt;
            while (!dq.empty() && dq.back() <= t - w) dq.pop_back();
            if (std::isfinite(v)) {
                while (!dq.empty() && x.at(i, dq.front()) >= v) dq.pop_front();
                dq.push_front(t);
            }
            if (t >= w - 1 && nanCnt == 0 && !dq.empty()) r.at(i, t) = x.at(i, dq.back());
        }
    }
    return r;
}

// 0 = 窗口最老位置
inline Mat ts_argmax(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    std::deque<int> dq;
    std::vector<unsigned char> bad(w, 0);
    for (int i = 0; i < x.N; ++i) {
        dq.clear();
        int nanCnt = 0;
        for (int t = 0; t < x.T; ++t) {
            int slot = t % w;
            if (t >= w && bad[slot]) --nanCnt;
            float v = x.at(i, t);
            bad[slot] = std::isfinite(v) ? 0 : 1;
            if (bad[slot]) ++nanCnt;
            while (!dq.empty() && dq.back() <= t - w) dq.pop_back();
            if (std::isfinite(v)) {
                while (!dq.empty() && x.at(i, dq.front()) <= v) dq.pop_front();
                dq.push_front(t);
            }
            if (t >= w - 1 && nanCnt == 0 && !dq.empty()) r.at(i, t) = (float)(w - 1 - (t - dq.back()));
        }
    }
    return r;
}

inline Mat ts_argmin(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    std::deque<int> dq;
    std::vector<unsigned char> bad(w, 0);
    for (int i = 0; i < x.N; ++i) {
        dq.clear();
        int nanCnt = 0;
        for (int t = 0; t < x.T; ++t) {
            int slot = t % w;
            if (t >= w && bad[slot]) --nanCnt;
            float v = x.at(i, t);
            bad[slot] = std::isfinite(v) ? 0 : 1;
            if (bad[slot]) ++nanCnt;
            while (!dq.empty() && dq.back() <= t - w) dq.pop_back();
            if (std::isfinite(v)) {
                while (!dq.empty() && x.at(i, dq.front()) >= v) dq.pop_front();
                dq.push_front(t);
            }
            if (t >= w - 1 && nanCnt == 0 && !dq.empty()) r.at(i, t) = (float)(w - 1 - (t - dq.back()));
        }
    }
    return r;
}

// 当前值在窗口内的分位（<= 当前值的比例）
inline Mat ts_rank(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    for (int i = 0; i < x.N; ++i) {
        for (int t = w - 1; t < x.T; ++t) {
            float cur = x.at(i, t);
            if (!std::isfinite(cur)) continue;
            int cnt = 0;
            bool bad = false;
            for (int k = 0; k < w; ++k) {
                float u = x.at(i, t - k);
                if (!std::isfinite(u)) { bad = true; break; }
                if (u <= cur) ++cnt;
            }
            if (!bad) r.at(i, t) = (float)cnt / (float)w;
        }
    }
    return r;
}

inline Mat ts_prod(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    for (int i = 0; i < x.N; ++i) {
        for (int t = w - 1; t < x.T; ++t) {
            double p = 1.0;
            bool bad = false;
            for (int k = 0; k < w; ++k) {
                float u = x.at(i, t - k);
                if (!std::isfinite(u)) { bad = true; break; }
                p *= u;
            }
            if (!bad) r.at(i, t) = (float)p;
        }
    }
    return r;
}

inline Mat decay_linear(const Mat& x, int w) {
    w = wr(w);
    Mat r(x.T, x.N);
    double denom = (double)w * (w + 1) / 2.0;
    for (int i = 0; i < x.N; ++i) {
        for (int t = w - 1; t < x.T; ++t) {
            double s = 0.0;
            bool bad = false;
            for (int k = 0; k < w; ++k) {
                float u = x.at(i, t - k);
                if (!std::isfinite(u)) { bad = true; break; }
                s += (double)(w - k) * u;
            }
            if (!bad) r.at(i, t) = (float)(s / denom);
        }
    }
    return r;
}

inline Mat delay(const Mat& x, int d) {
    Mat r(x.T, x.N);
    for (int i = 0; i < x.N; ++i)
        for (int t = d; t < x.T; ++t) r.at(i, t) = x.at(i, t - d);
    return r;
}

inline Mat delta(const Mat& x, int d) { return sub(x, delay(x, d)); }

#endif
