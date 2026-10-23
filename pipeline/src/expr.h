// 因子表达式 DSL：解析 + 求值
//
// 语法：前缀函数式，例如
//   div(sub(close, vwap), addc(vwap, 0.001))
//   ts_corr(rank(volume), rank(close), 20)
// 字段：open high low close vwap volume amount
#ifndef PIPELINE_EXPR_H
#define PIPELINE_EXPR_H

#include "panel.h"
#include <string>
#include <vector>
#include <cctype>
#include <cstdlib>
#include <map>

enum ExprOp {
    OP_FIELD = 1, OP_CONST,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_POW, OP_MAX, OP_MIN,
    OP_NEG, OP_ABS, OP_LOG, OP_SIGN, OP_SQRT,
    OP_GT, OP_LT, OP_GE, OP_LE, OP_EQ,
    OP_SEL,
    OP_RANK, OP_SCALE,
    OP_DELAY, OP_DELTA,
    OP_TSSUM, OP_TSMEAN, OP_TSSTD, OP_TSMIN, OP_TSMAX,
    OP_TSARGMAX, OP_TSARGMIN, OP_TSRANK, OP_TSPROD, OP_DECAY,
    OP_TSCORR, OP_TSCOV
};

struct ExprNode {
    int op;
    std::vector<ExprNode*> ch;
    double c;
    int w;
    int field;
    ExprNode(int o) : op(o), c(0), w(1), field(0) {}
};

static const char* FIELD_NAMES[] = {"open", "high", "low", "close", "vwap", "volume", "amount"};
static const int NFIELD = 7;

static inline const Mat& field_mat(const Panel& p, int f) {
    switch (f) {
        case 0: return p.open;
        case 1: return p.high;
        case 2: return p.low;
        case 3: return p.close;
        case 4: return p.vwap;
        case 5: return p.volume;
        default: return p.amount;
    }
}

// ---------------- 解析 ----------------
struct ExprParser {
    const std::string& s;
    size_t pos;
    std::string err;

    ExprParser(const std::string& str) : s(str), pos(0) {}

    void skip() { while (pos < s.size() && std::isspace((unsigned char)s[pos])) ++pos; }
    bool eat(char c) { skip(); if (pos < s.size() && s[pos] == c) { ++pos; return true; } return false; }

    std::string ident() {
        skip();
        size_t b = pos;
        while (pos < s.size() && (std::isalnum((unsigned char)s[pos]) || s[pos] == '_')) ++pos;
        return s.substr(b, pos - b);
    }

    ExprNode* number() {
        skip();
        size_t b = pos;
        if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) ++pos;
        while (pos < s.size() && (std::isdigit((unsigned char)s[pos]) || s[pos] == '.' ||
                                  s[pos] == 'e' || s[pos] == 'E' || s[pos] == '-' || s[pos] == '+')) ++pos;
        if (b == pos) return 0;
        ExprNode* n = new ExprNode(OP_CONST);
        n->c = atof(s.substr(b, pos - b).c_str());
        return n;
    }

    ExprNode* expr() {
        skip();
        if (pos >= s.size()) return 0;
        if (std::isdigit((unsigned char)s[pos]) || s[pos] == '-' || s[pos] == '.') return number();
        std::string name = ident();
        if (name.empty()) { err = "expect name at " + std::to_string(pos); return 0; }

        for (int f = 0; f < NFIELD; ++f) {
            if (name == FIELD_NAMES[f]) {
                ExprNode* n = new ExprNode(OP_FIELD);
                n->field = f;
                return n;
            }
        }
        ExprNode* n = make_op(name);
        if (!n) { err = "unknown op " + name; return 0; }
        if (!eat('(')) { err = "expect ( after " + name; return 0; }
        while (true) {
            ExprNode* a = expr();
            if (!a) return 0;
            n->ch.push_back(a);
            skip();
            if (eat(',')) continue;
            if (eat(')')) break;
            err = "expect , or ) in " + name;
            return 0;
        }
        return n;
    }

    // C++11 保证函数内 static 的初始化是线程安全的：
    // 之前用 "if (M.empty()) 再填充" 的惰性初始化，多线程并发解析时会写坏 map 导致段错误。
    static const std::map<std::string, int>& op_table() {
        static const std::map<std::string, int> M = build_table();
        return M;
    }

    static std::map<std::string, int> build_table() {
        std::map<std::string, int> m;
        const char* names[] = {"add","sub","mul","div","pow","max","min",
            "neg","abs","log","sign","sqrt","gt","lt","ge","le","eq","sel",
            "rank","scale","delay","delta","ts_sum","ts_mean","ts_std",
            "ts_min","ts_max","ts_argmax","ts_argmin","ts_rank","ts_prod",
            "decay","ts_corr","ts_cov"};
        int ops[] = {OP_ADD,OP_SUB,OP_MUL,OP_DIV,OP_POW,OP_MAX,OP_MIN,
            OP_NEG,OP_ABS,OP_LOG,OP_SIGN,OP_SQRT,OP_GT,OP_LT,OP_GE,OP_LE,OP_EQ,OP_SEL,
            OP_RANK,OP_SCALE,OP_DELAY,OP_DELTA,OP_TSSUM,OP_TSMEAN,OP_TSSTD,
            OP_TSMIN,OP_TSMAX,OP_TSARGMAX,OP_TSARGMIN,OP_TSRANK,OP_TSPROD,
            OP_DECAY,OP_TSCORR,OP_TSCOV};
        for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i) m[names[i]] = ops[i];
        return m;
    }

    static ExprNode* make_op(const std::string& name) {
        const std::map<std::string, int>& M = op_table();
        std::map<std::string, int>::const_iterator it = M.find(name);
        if (it == M.end()) return 0;
        return new ExprNode(it->second);
    }
};

// ---------------- 求值 ----------------
static inline bool is_const(const ExprNode* n) { return n->op == OP_CONST; }

static inline Mat mat_add(const Mat& a, const Mat& b) { return add(a, b); }
static inline Mat mat_sub(const Mat& a, const Mat& b) { return sub(a, b); }
static inline Mat mat_mul(const Mat& a, const Mat& b) { return mul(a, b); }
static inline Mat mat_div(const Mat& a, const Mat& b) { return divv(a, b); }

static Mat mat_sel(const Mat& c, const Mat& a, const Mat& b) {
    Mat r(c.T, c.N);
    for (size_t k = 0; k < r.d.size(); ++k)
        r.d[k] = (c.d[k] != 0.0f && !std::isnan(c.d[k])) ? a.d[k] : b.d[k];
    return r;
}

static Mat eval_expr(const ExprNode* n, const Panel& p);

static Mat eval2(const ExprNode* a, const ExprNode* b, const Panel& p, int op) {
    // 标量优化：一侧是常数时走标量路径
    if (is_const(a)) {
        double c = a->c;
        Mat x = eval_expr(b, p);
        switch (op) {
            case OP_ADD: return addc(x, (float)c);
            case OP_SUB: return rsubc((float)c, x);
            case OP_MUL: return mulc(x, (float)c);
            case OP_DIV: return rdivc((float)c, x);
            case OP_POW: return powc(x, (float)c);
            case OP_GT: return bool10(gtc(x, (float)c));       // c > x  => x < c
            case OP_LT: return bool10(ltc(x, (float)c));       // c < x  => x > c
            case OP_GE: return bool10(gec(x, (float)c));
            case OP_LE: return bool10(lec(x, (float)c));
            case OP_EQ: return bool10(lec(absv(subc(x, (float)c)), 1e-12f));
        }
    }
    if (is_const(b)) {
        double c = b->c;
        Mat x = eval_expr(a, p);
        switch (op) {
            case OP_ADD: return addc(x, (float)c);
            case OP_SUB: return subc(x, (float)c);
            case OP_MUL: return mulc(x, (float)c);
            case OP_DIV: return divc(x, (float)c);
            case OP_POW: return powc(x, (float)c);
            case OP_GT: return bool10(gtc(x, (float)c));
            case OP_LT: return bool10(ltc(x, (float)c));
            case OP_GE: return bool10(gec(x, (float)c));
            case OP_LE: return bool10(lec(x, (float)c));
            case OP_EQ: return bool10(lec(absv(subc(x, (float)c)), 1e-12f));
        }
    }
    Mat x = eval_expr(a, p);
    Mat y = eval_expr(b, p);
    switch (op) {
        case OP_ADD: return add(x, y);
        case OP_SUB: return sub(x, y);
        case OP_MUL: return mul(x, y);
        case OP_DIV: return divv(x, y);
        case OP_POW: return powv(x, y);
        case OP_GT: return bool10(gt(x, y));
        case OP_LT: return bool10(lt(x, y));
        case OP_GE: return bool10(m_or(gt(x, y), lec(absv(sub(x, y)), 0.0f)));
        case OP_LE: return bool10(m_or(lt(x, y), lec(absv(sub(x, y)), 0.0f)));
        case OP_EQ: return bool10(lec(absv(sub(x, y)), 1e-12f));
    }
    Mat bad(x.T, x.N);
    return bad;
}

static Mat eval_expr(const ExprNode* n, const Panel& p) {
    switch (n->op) {
        case OP_FIELD: return field_mat(p, n->field);
        case OP_CONST: { Mat r(p.T, p.N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = (float)n->c; return r; }
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_POW:
        case OP_GT: case OP_LT: case OP_GE: case OP_LE: case OP_EQ:
            return eval2(n->ch[0], n->ch[1], p, n->op);
        case OP_MAX: return max2(eval_expr(n->ch[0], p), eval_expr(n->ch[1], p));
        case OP_MIN: return min2(eval_expr(n->ch[0], p), eval_expr(n->ch[1], p));
        case OP_NEG: return negv(eval_expr(n->ch[0], p));
        case OP_ABS: return absv(eval_expr(n->ch[0], p));
        case OP_LOG: return logv(eval_expr(n->ch[0], p));
        case OP_SIGN: return signv(eval_expr(n->ch[0], p));
        case OP_SQRT: return sqrtv(eval_expr(n->ch[0], p));
        case OP_SEL: {
            Mat c = eval_expr(n->ch[0], p);
            Mat a = eval_expr(n->ch[1], p);
            Mat b = eval_expr(n->ch[2], p);
            return mat_sel(c, a, b);
        }
        case OP_RANK: return rankv(eval_expr(n->ch[0], p));
        case OP_SCALE: return scalev(eval_expr(n->ch[0], p));
        case OP_DELAY: return delay(eval_expr(n->ch[0], p), n->w);
        case OP_DELTA: return delta(eval_expr(n->ch[0], p), n->w);
        case OP_TSSUM: return ts_sum(eval_expr(n->ch[0], p), n->w);
        case OP_TSMEAN: return ts_mean(eval_expr(n->ch[0], p), n->w);
        case OP_TSSTD: return ts_std(eval_expr(n->ch[0], p), n->w);
        case OP_TSMIN: return ts_min(eval_expr(n->ch[0], p), n->w);
        case OP_TSMAX: return ts_max(eval_expr(n->ch[0], p), n->w);
        case OP_TSARGMAX: return ts_argmax(eval_expr(n->ch[0], p), n->w);
        case OP_TSARGMIN: return ts_argmin(eval_expr(n->ch[0], p), n->w);
        case OP_TSRANK: return ts_rank(eval_expr(n->ch[0], p), n->w);
        case OP_TSPROD: return ts_prod(eval_expr(n->ch[0], p), n->w);
        case OP_DECAY: return decay_linear(eval_expr(n->ch[0], p), n->w);
        case OP_TSCORR: return ts_corr(eval_expr(n->ch[0], p), eval_expr(n->ch[1], p), n->w);
        case OP_TSCOV: return ts_cov(eval_expr(n->ch[0], p), eval_expr(n->ch[1], p), n->w);
    }
    Mat bad(p.T, p.N);
    return bad;
}

// 数值型子节点：如果是 ts_* / delay / delta / decay / ts_corr，最后一个参数是窗口
static void bind_windows(ExprNode* n, std::string& err) {
    static const int NEEDS_W[] = {OP_DELAY, OP_DELTA, OP_TSSUM, OP_TSMEAN, OP_TSSTD,
        OP_TSMIN, OP_TSMAX, OP_TSARGMAX, OP_TSARGMIN, OP_TSRANK, OP_TSPROD, OP_DECAY,
        OP_TSCORR, OP_TSCOV};
    for (size_t i = 0; i < sizeof(NEEDS_W)/sizeof(NEEDS_W[0]); ++i) {
        if (n->op == NEEDS_W[i]) {
            int nargs = (n->op == OP_TSCORR || n->op == OP_TSCOV) ? 3 : 2;
            if ((int)n->ch.size() != nargs) { err = "bad arity for windowed op"; return; }
            ExprNode* last = n->ch.back();
            if (last->op != OP_CONST) { err = "window must be constant"; return; }
            n->w = (int)(last->c + 0.5);
            if (n->w < 1) n->w = 1;
            n->ch.pop_back();
        }
    }
    for (size_t i = 0; i < n->ch.size(); ++i) bind_windows(n->ch[i], err);
}

#endif
