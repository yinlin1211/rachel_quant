// 82 条 Alpha101 公式（48/56/58/59/63/67/69/70/76/79/80/82/87/89/90/91/93/97/100 依赖
// 行业分类或市值，本数据没有，故不实现；与 KunQuant 的 all_alpha 列表一致）。
#include "common.h"

struct AllData {
    const Mat *open, *high, *low, *close, *vwap, *volume, *amount, *returns;
    int T, N;
};

static Mat full(int T, int N, float v) { Mat r(T, N); for (size_t k = 0; k < r.d.size(); ++k) r.d[k] = v; return r; }
static Mat one_minus(const Mat& x) { return negv(subc(x, 1.0f)); }
static Mat midof(const AllData& d) { return mulc(add(*d.high, *d.low), 0.5f); }

static Mat f001(const AllData& d) {
    const Mat& ret = *d.returns;
    Mat sd = ts_std(ret, 20);
    Mat sel = select(ltc(ret, 0.0f), sd, *d.close);
    Mat sq = mul(sel, sel);
    return subc(rankv(ts_argmax(sq, 5)), 0.5f);
}

static Mat f002(const AllData& d) {
    Mat lv = logv(*d.volume);
    Mat a = rankv(delta(lv, 2));
    Mat b = rankv(divv(sub(*d.close, *d.open), *d.open));
    return negv(ts_corr(a, b, 6));
}

static Mat f003(const AllData& d) {
    return negv(ts_corr(rankv(*d.open), rankv(*d.volume), 10));
}

static Mat f004(const AllData& d) {
    return negv(ts_rank(rankv(*d.low), 9));
}

static Mat f005(const AllData& d) {
    Mat a = rankv(sub(*d.open, ts_mean(*d.vwap, 10)));
    Mat b = negv(absv(rankv(sub(*d.close, *d.vwap))));
    return mul(a, b);
}

static Mat f006(const AllData& d) {
    return negv(ts_corr(*d.open, *d.volume, 10));
}

static Mat f007(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat dc = delta(*d.close, 7);
    Mat inner = negv(mul(ts_rank(absv(dc), 60), signv(dc)));
    Mask cond = lt(a20, *d.volume);
    return select(cond, inner, full(d.T, d.N, -1.0f));
}

static Mat f008(const AllData& d) {
    Mat p = mul(ts_sum(*d.open, 5), ts_sum(*d.returns, 5));
    return negv(rankv(sub(p, delay(p, 10))));
}

static Mat f009(const AllData& d) {
    Mat dc = delta(*d.close, 1);
    Mask c1 = gtc(ts_min(dc, 5), 0.0f);
    Mask c2 = ltc(ts_max(dc, 5), 0.0f);
    return select(m_or(c1, c2), dc, negv(dc));
}

static Mat f010(const AllData& d) {
    Mat dc = delta(*d.close, 1);
    Mask c1 = gtc(ts_min(dc, 4), 0.0f);
    Mask c2 = ltc(ts_max(dc, 4), 0.0f);
    return select(m_or(c1, c2), dc, negv(dc));
}

static Mat f011(const AllData& d) {
    Mat vc = sub(*d.vwap, *d.close);
    Mat a = add(rankv(ts_max(vc, 3)), rankv(ts_min(vc, 3)));
    return mul(a, rankv(delta(*d.volume, 3)));
}

static Mat f012(const AllData& d) {
    return mul(signv(delta(*d.volume, 1)), negv(delta(*d.close, 1)));
}

static Mat f013(const AllData& d) {
    return negv(rankv(ts_cov(rankv(*d.close), rankv(*d.volume), 5)));
}

static Mat f014(const AllData& d) {
    return mul(negv(rankv(delta(*d.returns, 3))), ts_corr(*d.open, *d.volume, 10));
}

static Mat f015(const AllData& d) {
    return negv(ts_sum(rankv(ts_corr(rankv(*d.high), rankv(*d.volume), 3)), 3));
}

static Mat f016(const AllData& d) {
    return negv(rankv(ts_cov(rankv(*d.high), rankv(*d.volume), 5)));
}

static Mat f017(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat p1 = negv(rankv(ts_rank(*d.close, 10)));
    Mat p2 = rankv(delta(delta(*d.close, 1), 1));
    Mat p3 = rankv(ts_rank(divv(*d.volume, a20), 5));
    return mul(mul(p1, p2), p3);
}

static Mat f018(const AllData& d) {
    Mat co = sub(*d.close, *d.open);
    Mat x = add(add(ts_std(absv(co), 5), co), ts_corr(*d.close, *d.open, 10));
    return negv(rankv(x));
}

static Mat f019(const AllData& d) {
    Mat x = add(sub(*d.close, delay(*d.close, 7)), delta(*d.close, 7));
    Mat y = addc(rankv(addc(ts_sum(*d.returns, 250), 1.0f)), 1.0f);
    return mul(negv(signv(x)), y);
}

static Mat f020(const AllData& d) {
    Mat a = negv(rankv(sub(*d.open, delay(*d.high, 1))));
    Mat b = rankv(sub(*d.open, delay(*d.close, 1)));
    Mat c = rankv(sub(*d.open, delay(*d.low, 1)));
    return mul(mul(a, b), c);
}

static Mat f021(const AllData& d) {
    Mat m2 = ts_mean(*d.close, 2);
    Mat m8 = ts_mean(*d.close, 8);
    Mat s8 = ts_std(*d.close, 8);
    Mat a20 = ts_mean(*d.volume, 20);
    Mask a = lt(add(m8, s8), m2);
    Mask c = lt(m2, sub(m8, s8));
    Mask dd = gec(divv(*d.volume, a20), 1.0f);
    Mat out = selectc(m_or(c, dd), 1.0f, -1.0f);
    return select(a, full(d.T, d.N, -1.0f), out);
}

static Mat f022(const AllData& d) {
    Mat x = mulc(delta(ts_corr(*d.high, *d.volume, 5), 5), -1.0f);
    return mul(x, rankv(ts_std(*d.close, 20)));
}

static Mat f023(const AllData& d) {
    Mask cond = lt(ts_mean(*d.high, 20), *d.high);
    return select(cond, negv(delta(*d.high, 2)), full(d.T, d.N, 0.0f));
}

static Mat f024(const AllData& d) {
    Mat c100 = ts_mean(*d.close, 100);
    Mat r = divv(delta(c100, 100), delay(*d.close, 100));
    Mask cond = lec(r, 0.05f);
    Mat a = negv(sub(*d.close, ts_min(*d.close, 100)));
    Mat b = negv(delta(*d.close, 3));
    return select(cond, a, b);
}

static Mat f025(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat x = mulc(*d.returns, -1.0f);
    x = mul(x, a20);
    x = mul(x, *d.vwap);
    x = mul(x, sub(*d.high, *d.close));
    return rankv(x);
}

static Mat f026(const AllData& d) {
    Mat c = ts_corr(ts_rank(*d.volume, 5), ts_rank(*d.high, 5), 5);
    return negv(ts_max(c, 3));
}

static Mat f027(const AllData& d) {
    Mat c = ts_corr(rankv(*d.volume), rankv(*d.vwap), 6);
    Mat x = rankv(divc(ts_mean(c, 2), 2.0f));
    return selectc(gtc(x, 0.5f), -1.0f, 1.0f);
}

static Mat f028(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat x = add(ts_corr(a20, *d.low, 5), sub(midof(d), *d.close));
    return scalev(x);
}

static Mat f029(const AllData& d) {
    Mat x1 = rankv(delta(subc(*d.close, 1.0f), 5));
    Mat x2 = negv(x1);
    Mat x3 = rankv(x2);
    Mat x4 = rankv(x3);
    Mat x5 = ts_min(x4, 2);
    Mat x6 = ts_sum(x5, 1);
    Mat x7 = logv(x6);
    Mat x8 = scalev(x7);
    Mat x9 = rankv(x8);
    Mat x10 = rankv(x9);
    Mat x11 = ts_prod(x10, 1);
    Mat x12 = ts_min(x11, 5);
    Mat p2 = ts_rank(delay(negv(*d.returns), 6), 5);
    return add(x12, p2);
}

static Mat f030(const AllData& d) {
    Mat dc = delta(*d.close, 1);
    Mat inner = add(add(signv(dc), signv(delay(dc, 1))), signv(delay(dc, 2)));
    Mat a = one_minus(rankv(inner));
    Mat b = divv(ts_sum(*d.volume, 5), ts_sum(*d.volume, 20));
    return mul(a, b);
}

static Mat f031(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat x = negv(rankv(rankv(delta(*d.close, 10))));
    Mat p1 = rankv(rankv(rankv(decay_linear(x, 10))));
    Mat p2 = rankv(negv(delta(*d.close, 3)));
    Mat p3 = signv(scalev(ts_corr(a20, *d.low, 12)));
    return add(add(p1, p2), p3);
}

static Mat f032(const AllData& d) {
    Mat a = scalev(sub(ts_mean(*d.close, 7), *d.close));
    Mat b = mulc(scalev(ts_corr(*d.vwap, delay(*d.close, 5), 230)), 20.0f);
    return add(a, b);
}

static Mat f033(const AllData& d) {
    return rankv(subc(divv(*d.open, *d.close), 1.0f));
}

static Mat f034(const AllData& d) {
    Mat x = divv(ts_std(*d.returns, 2), ts_std(*d.returns, 5));
    Mat a = one_minus(rankv(x));
    Mat b = one_minus(rankv(delta(*d.close, 1)));
    return rankv(add(a, b));
}

static Mat f035(const AllData& d) {
    Mat chl = add(*d.close, sub(*d.high, *d.low));
    Mat a = ts_rank(*d.volume, 32);
    Mat b = one_minus(ts_rank(chl, 16));
    Mat c = one_minus(ts_rank(*d.returns, 32));
    return mul(mul(a, b), c);
}

static Mat f036(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat co = sub(*d.close, *d.open);
    Mat t1 = mulc(rankv(ts_corr(co, delay(*d.volume, 1), 15)), 2.21f);
    Mat t2 = mulc(rankv(negv(co)), 0.7f);
    Mat t3 = mulc(rankv(ts_rank(delay(negv(*d.returns), 6), 5)), 0.73f);
    Mat t4 = rankv(absv(ts_corr(*d.vwap, a20, 6)));
    Mat t5 = mulc(rankv(mul(sub(ts_mean(*d.close, 200), *d.open), co)), 0.6f);
    return add(add(add(add(t1, t2), t3), t4), t5);
}

static Mat f037(const AllData& d) {
    Mat a = rankv(ts_corr(delay(sub(*d.open, *d.close), 1), *d.close, 200));
    return add(a, rankv(sub(*d.open, *d.close)));
}

static Mat f038(const AllData& d) {
    return mul(negv(rankv(ts_rank(*d.close, 10))), rankv(divv(*d.close, *d.open)));
}

static Mat f039(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat a = one_minus(rankv(decay_linear(divv(*d.volume, a20), 9)));
    Mat x = mul(delta(*d.close, 7), a);
    Mat y = addc(rankv(ts_sum(*d.returns, 250)), 1.0f);
    return mul(negv(rankv(x)), y);
}

static Mat f040(const AllData& d) {
    return mul(negv(rankv(ts_std(*d.high, 10))), ts_corr(*d.high, *d.volume, 10));
}

static Mat f041(const AllData& d) {
    return sub(sqrtv(mul(*d.high, *d.low)), *d.vwap);
}

static Mat f042(const AllData& d) {
    return divv(rankv(sub(*d.vwap, *d.close)), rankv(add(*d.vwap, *d.close)));
}

static Mat f043(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    return mul(ts_rank(divv(*d.volume, a20), 20), ts_rank(negv(delta(*d.close, 7)), 8));
}

static Mat f044(const AllData& d) {
    return negv(ts_corr(*d.high, rankv(*d.volume), 5));
}

static Mat f045(const AllData& d) {
    Mat a = rankv(ts_mean(delay(*d.close, 5), 20));
    Mat b = ts_corr(*d.close, *d.volume, 2);
    Mat c = rankv(ts_corr(ts_sum(*d.close, 5), ts_sum(*d.close, 20), 2));
    return negv(mul(mul(a, b), c));
}

static Mat f046(const AllData& d) {
    Mat inner = sub(divc(sub(delay(*d.close, 20), delay(*d.close, 10)), 10.0f),
                    divc(sub(delay(*d.close, 10), *d.close), 10.0f));
    Mat base = negv(delta(*d.close, 1));
    Mat out = select(ltc(inner, 0.0f), full(d.T, d.N, 1.0f), base);
    return select(gtc(inner, 0.25f), full(d.T, d.N, -1.0f), out);
}

static Mat f047(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat a = divv(mul(rankv(rdivc(1.0f, *d.close)), *d.volume), a20);
    Mat b = divv(mul(*d.high, rankv(sub(*d.high, *d.close))), ts_mean(*d.high, 5));
    Mat c = rankv(sub(*d.vwap, delay(*d.vwap, 5)));
    return sub(mul(a, b), c);
}

static Mat f049(const AllData& d) {
    Mat inner = sub(divc(sub(delay(*d.close, 20), delay(*d.close, 10)), 10.0f),
                    divc(sub(delay(*d.close, 10), *d.close), 10.0f));
    return select(ltc(inner, -0.1f), full(d.T, d.N, 1.0f), negv(delta(*d.close, 1)));
}

static Mat f050(const AllData& d) {
    Mat c = ts_corr(rankv(*d.volume), rankv(*d.vwap), 5);
    return negv(ts_max(rankv(c), 5));
}

static Mat f051(const AllData& d) {
    Mat inner = sub(divc(sub(delay(*d.close, 20), delay(*d.close, 10)), 10.0f),
                    divc(sub(delay(*d.close, 10), *d.close), 10.0f));
    return select(ltc(inner, -0.05f), full(d.T, d.N, 1.0f), negv(delta(*d.close, 1)));
}

static Mat f052(const AllData& d) {
    Mat a = negv(delta(ts_min(*d.low, 5), 5));
    Mat b = rankv(divc(sub(ts_sum(*d.returns, 240), ts_sum(*d.returns, 20)), 220.0f));
    Mat c = ts_rank(*d.volume, 5);
    return mul(mul(a, b), c);
}

static Mat f053(const AllData& d) {
    Mat inner = sub(*d.close, *d.low);
    Mat x = divv(sub(inner, sub(*d.high, *d.close)), inner);
    return negv(delta(x, 9));
}

static Mat f054(const AllData& d) {
    Mat num = mul(sub(*d.low, *d.close), powc(*d.open, 5.0f));
    Mat den = mul(sub(*d.low, *d.high), powc(*d.close, 5.0f));
    return negv(divv(num, den));
}

static Mat f055(const AllData& d) {
    Mat den = sub(ts_max(*d.high, 12), ts_min(*d.low, 12));
    Mat inner = divv(sub(*d.close, ts_min(*d.low, 12)), den);
    return negv(ts_corr(rankv(inner), rankv(*d.volume), 6));
}

static Mat f057(const AllData& d) {
    Mat den = decay_linear(rankv(ts_argmax(*d.close, 30)), 2);
    return negv(divv(sub(*d.close, *d.vwap), den));
}

static Mat f060(const AllData& d) {
    Mat den = sub(*d.high, *d.low);
    Mat inner = divv(mul(sub(sub(*d.close, *d.low), sub(*d.high, *d.close)), *d.volume), den);
    Mat a = scalev(rankv(inner));
    Mat b = scalev(rankv(ts_argmax(*d.close, 10)));
    return negv(sub(mulc(a, 2.0f), b));
}

static Mat f061(const AllData& d) {
    Mat a = rankv(sub(*d.vwap, ts_min(*d.vwap, 16)));
    Mat b = rankv(ts_corr(*d.vwap, ts_mean(*d.volume, 180), 18));
    return bool10(lt(a, b));
}

static Mat f062(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat v1 = rankv(ts_corr(*d.vwap, ts_sum(a20, 22), 10));
    Mat ro = rankv(*d.open);
    Mat v2 = add(ro, ro);
    Mat v3 = add(rankv(midof(d)), rankv(*d.high));
    Mat v4 = bool10(lt(v2, v3));
    Mat v5 = bool10(lt(v1, rankv(v4)));
    return negv(v5);
}

static Mat f064(const AllData& d) {
    Mat adv120 = ts_mean(*d.volume, 120);
    Mat x = add(mulc(*d.open, 0.178404f), mulc(*d.low, 0.821596f));
    Mat a = rankv(ts_corr(ts_sum(x, 13), ts_sum(adv120, 13), 17));
    Mat y = add(mulc(midof(d), 0.178404f), mulc(*d.vwap, 0.821596f));
    Mat b = rankv(delta(y, 4));
    return negv(bool10(lt(a, b)));
}

static Mat f065(const AllData& d) {
    Mat adv60 = ts_mean(*d.volume, 60);
    Mat x = add(mulc(*d.open, 0.00817205f), mulc(*d.vwap, 0.99182795f));
    Mat a = rankv(ts_corr(x, ts_sum(adv60, 9), 6));
    Mat b = rankv(sub(*d.open, ts_min(*d.open, 14)));
    return negv(bool10(lt(a, b)));
}

static Mat f066(const AllData& d) {
    Mat p1 = rankv(decay_linear(delta(*d.vwap, 4), 7));
    Mat inner = divv(sub(*d.low, *d.vwap), sub(*d.open, midof(d)));
    Mat p2 = ts_rank(decay_linear(inner, 11), 7);
    return negv(add(p1, p2));
}

static Mat f068(const AllData& d) {
    Mat adv15 = ts_mean(*d.volume, 15);
    Mat a = ts_rank(ts_corr(rankv(*d.high), rankv(adv15), 9), 14);
    Mat x = add(mulc(*d.close, 0.518371f), mulc(*d.low, 0.481629f));
    Mat b = rankv(delta(x, 1));
    return negv(bool10(lt(a, b)));
}

static Mat f071(const AllData& d) {
    Mat adv180 = ts_mean(*d.volume, 180);
    Mat p1 = ts_rank(decay_linear(ts_corr(ts_rank(*d.close, 3), ts_rank(adv180, 12), 18), 4), 16);
    Mat inner = powc(rankv(sub(sub(add(*d.low, *d.open), *d.vwap), *d.vwap)), 2.0f);
    Mat p2 = ts_rank(decay_linear(inner, 16), 4);
    return max2(p1, p2);
}

static Mat f072(const AllData& d) {
    Mat adv40 = ts_mean(*d.volume, 40);
    Mat a = rankv(decay_linear(ts_corr(midof(d), adv40, 9), 10));
    Mat b = rankv(decay_linear(ts_corr(ts_rank(*d.vwap, 4), ts_rank(*d.volume, 19), 7), 3));
    return divv(a, b);
}

static Mat f073(const AllData& d) {
    Mat p1 = rankv(decay_linear(delta(*d.vwap, 5), 3));
    Mat x = add(mulc(*d.open, 0.147155f), mulc(*d.low, 0.852845f));
    Mat inner = mulc(divv(delta(x, 2), x), -1.0f);
    Mat p2 = ts_rank(decay_linear(inner, 3), 17);
    return negv(max2(p1, p2));
}

static Mat f074(const AllData& d) {
    Mat adv30 = ts_mean(*d.volume, 30);
    Mat a = rankv(ts_corr(*d.close, ts_sum(adv30, 37), 15));
    Mat x = add(mulc(*d.high, 0.0261661f), mulc(*d.vwap, 0.9738339f));
    Mat b = rankv(ts_corr(rankv(x), rankv(*d.volume), 11));
    return negv(bool10(lt(a, b)));
}

static Mat f075(const AllData& d) {
    Mat adv50 = ts_mean(*d.volume, 50);
    Mat a = rankv(ts_corr(*d.vwap, *d.volume, 4));
    Mat b = rankv(ts_corr(rankv(*d.low), rankv(adv50), 12));
    return bool10(lt(a, b));
}

static Mat f077(const AllData& d) {
    Mat adv40 = ts_mean(*d.volume, 40);
    Mat p1 = rankv(decay_linear(sub(midof(d), *d.vwap), 20));
    Mat p2 = rankv(decay_linear(ts_corr(midof(d), adv40, 3), 6));
    return min2(p1, p2);
}

static Mat f078(const AllData& d) {
    Mat adv40 = ts_mean(*d.volume, 40);
    Mat x = add(mulc(*d.low, 0.352233f), mulc(*d.vwap, 0.647767f));
    Mat a = rankv(ts_corr(ts_sum(x, 20), ts_sum(adv40, 20), 7));
    Mat b = rankv(ts_corr(rankv(*d.vwap), rankv(*d.volume), 6));
    return powv(a, b);
}

static Mat f081(const AllData& d) {
    Mat adv10 = ts_mean(*d.volume, 10);
    Mat inner = powc(rankv(ts_corr(*d.vwap, ts_sum(adv10, 50), 8)), 4.0f);
    Mat prod = ts_prod(rankv(inner), 15);
    Mat a = rankv(logv(prod));
    Mat b = rankv(ts_corr(rankv(*d.vwap), rankv(*d.volume), 5));
    return negv(bool10(lt(a, b)));
}

static Mat f083(const AllData& d) {
    Mat rng = divv(sub(*d.high, *d.low), ts_mean(*d.close, 5));
    Mat a = rankv(delay(rng, 2));
    Mat b = rankv(rankv(*d.volume));
    Mat den = divv(rng, sub(*d.vwap, *d.close));
    return divv(mul(a, b), den);
}

static Mat f084(const AllData& d) {
    return spow(ts_rank(sub(*d.vwap, ts_max(*d.vwap, 15)), 21), delta(*d.close, 5));
}

static Mat f085(const AllData& d) {
    Mat adv30 = ts_mean(*d.volume, 30);
    Mat x = add(mulc(*d.high, 0.876703f), mulc(*d.close, 0.123297f));
    Mat a = rankv(ts_corr(x, adv30, 10));
    Mat b = rankv(ts_corr(ts_rank(midof(d), 4), ts_rank(*d.volume, 10), 7));
    return powv(a, b);
}

static Mat f086(const AllData& d) {
    Mat a20 = ts_mean(*d.volume, 20);
    Mat a = ts_rank(ts_corr(*d.close, ts_sum(a20, 15), 6), 20);
    Mat b = rankv(sub(*d.close, *d.vwap));
    return negv(bool10(lt(a, b)));
}

static Mat f088(const AllData& d) {
    Mat p1 = rankv(decay_linear(sub(add(rankv(*d.open), rankv(*d.low)),
                                    add(rankv(*d.high), rankv(*d.close))), 8));
    Mat adv60 = ts_mean(*d.volume, 60);
    Mat p2 = ts_rank(decay_linear(ts_corr(ts_rank(*d.close, 8), ts_rank(adv60, 21), 8), 7), 3);
    return min2(p1, p2);
}

static Mat f092(const AllData& d) {
    Mat adv30 = ts_mean(*d.volume, 30);
    Mask cond = lt(add(midof(d), *d.close), add(*d.low, *d.open));
    Mat p1 = ts_rank(decay_linear(bool10(cond), 15), 19);
    Mat p2 = ts_rank(decay_linear(ts_corr(rankv(*d.low), rankv(adv30), 8), 7), 7);
    return min2(p1, p2);
}

static Mat f094(const AllData& d) {
    Mat adv60 = ts_mean(*d.volume, 60);
    Mat base = rankv(sub(*d.vwap, ts_min(*d.vwap, 12)));
    Mat expo = ts_rank(ts_corr(ts_rank(*d.vwap, 20), ts_rank(adv60, 4), 18), 3);
    return negv(powv(base, expo));
}

static Mat f095(const AllData& d) {
    Mat adv40 = ts_mean(*d.volume, 40);
    Mat a = rankv(sub(*d.open, ts_min(*d.open, 12)));
    Mat inner = powc(rankv(ts_corr(ts_sum(midof(d), 19), ts_sum(adv40, 19), 13)), 5.0f);
    Mat b = ts_rank(inner, 12);
    return bool10(lt(a, b));
}

static Mat f096(const AllData& d) {
    Mat adv60 = ts_mean(*d.volume, 60);
    Mat p1 = ts_rank(decay_linear(ts_corr(rankv(*d.vwap), rankv(*d.volume), 4), 4), 8);
    Mat inner = ts_argmax(ts_corr(ts_rank(*d.close, 7), ts_rank(adv60, 4), 4), 13);
    Mat p2 = ts_rank(decay_linear(inner, 14), 13);
    return negv(max2(p1, p2));
}

static Mat f098(const AllData& d) {
    Mat adv5 = ts_mean(*d.volume, 5);
    Mat adv15 = ts_mean(*d.volume, 15);
    Mat p1 = rankv(decay_linear(ts_corr(*d.vwap, ts_sum(adv5, 26), 5), 7));
    Mat inner = ts_rank(ts_argmin(ts_corr(rankv(*d.open), rankv(adv15), 21), 9), 7);
    Mat p2 = rankv(decay_linear(inner, 8));
    return sub(p1, p2);
}

static Mat f099(const AllData& d) {
    Mat adv60 = ts_mean(*d.volume, 60);
    Mat a = rankv(ts_corr(ts_sum(midof(d), 20), ts_sum(adv60, 20), 9));
    Mat b = rankv(ts_corr(*d.low, *d.volume, 6));
    return negv(bool10(lt(a, b)));
}

static Mat f101(const AllData& d) {
    return divv(sub(*d.close, *d.open), addc(sub(*d.high, *d.low), 0.001f));
}

// 名称 / 函数表
struct AlphaDef { const char* name; Mat (*fn)(const AllData&); };

static const AlphaDef ALPHAS[] = {
    {"alpha001", f001}, {"alpha002", f002}, {"alpha003", f003}, {"alpha004", f004},
    {"alpha005", f005}, {"alpha006", f006}, {"alpha007", f007}, {"alpha008", f008},
    {"alpha009", f009}, {"alpha010", f010}, {"alpha011", f011}, {"alpha012", f012},
    {"alpha013", f013}, {"alpha014", f014}, {"alpha015", f015}, {"alpha016", f016},
    {"alpha017", f017}, {"alpha018", f018}, {"alpha019", f019}, {"alpha020", f020},
    {"alpha021", f021}, {"alpha022", f022}, {"alpha023", f023}, {"alpha024", f024},
    {"alpha025", f025}, {"alpha026", f026}, {"alpha027", f027}, {"alpha028", f028},
    {"alpha029", f029}, {"alpha030", f030}, {"alpha031", f031}, {"alpha032", f032},
    {"alpha033", f033}, {"alpha034", f034}, {"alpha035", f035}, {"alpha036", f036},
    {"alpha037", f037}, {"alpha038", f038}, {"alpha039", f039}, {"alpha040", f040},
    {"alpha041", f041}, {"alpha042", f042}, {"alpha043", f043}, {"alpha044", f044},
    {"alpha045", f045}, {"alpha046", f046}, {"alpha047", f047}, {"alpha049", f049},
    {"alpha050", f050}, {"alpha051", f051}, {"alpha052", f052}, {"alpha053", f053},
    {"alpha054", f054}, {"alpha055", f055}, {"alpha057", f057}, {"alpha060", f060},
    {"alpha061", f061}, {"alpha062", f062}, {"alpha064", f064}, {"alpha065", f065},
    {"alpha066", f066}, {"alpha068", f068}, {"alpha071", f071}, {"alpha072", f072},
    {"alpha073", f073}, {"alpha074", f074}, {"alpha075", f075}, {"alpha077", f077},
    {"alpha078", f078}, {"alpha081", f081}, {"alpha083", f083}, {"alpha084", f084},
    {"alpha085", f085}, {"alpha086", f086}, {"alpha088", f088}, {"alpha092", f092},
    {"alpha094", f094}, {"alpha095", f095}, {"alpha096", f096}, {"alpha098", f098},
    {"alpha099", f099}, {"alpha101", f101},
};

