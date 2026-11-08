#pragma once


//////////////////// 时间操作函数 ///////////////////////
inline
int hhmmss_to_s(int hhmmss) {
    return hhmmss / 10000 * 3600 + (hhmmss / 100 % 100) * 60 + hhmmss % 100;
}

inline
int s_to_hhmmss(int s) {
    return s / 3600 * 10000 + s % 3600 / 60 * 100 + s % 60;
}

inline
int hhmmss_to_ms(unsigned hmsint) {
    int ms;
    unsigned hh;
    unsigned mm;
    unsigned ss;

    ss = hmsint % 100;
    mm = (hmsint - ss) / 100 % 100;
    hh = hmsint / 10000;

    ms = (hh * 3600 + mm * 60 + ss) * 1000;

    return (ms);
}

inline
int ms_to_hhmmss(unsigned ms) {
    int sec;
    int ss;
    int mm;
    int hh;
    int hhmmss;

    if ((unsigned int)(-1) == ms) {
        hhmmss = -1;
    }
    else {
        sec = ms / 1000;
        ss = sec % 60;
        mm = sec / 60;
        mm = mm % 60;
        hh = sec / 3600;

        hhmmss = hh * 10000 + mm * 100 + ss;
    }
    return(hhmmss);
}

inline
int hhmmssxxx_to_ms(int hhmmssxxx) {
    return hhmmss_to_s(hhmmssxxx / 1000) * 1000 + hhmmssxxx % 1000;
}

inline
int ms_to_hhmmssxxx(int ms) {
    return s_to_hhmmss(ms / 1000) * 1000 + ms % 1000;
}

//与交易时间对齐

//转换成秒
inline
int hhmmss_to_normal_time(int hhmmss) {
    //int s  = hhmmss_to_s(hhmmss) - hhmmss_to_s(93000);
    int s = hhmmss / 10000 * 3600 + (hhmmss / 100 % 100) * 60 + hhmmss % 100 - 34200;
    if (s < 0)
        return -1;
    //else if(s <= 120 * 60) 
    else if (s <= 7200)
        return s;
    //else if(s < 210 * 60)
    else if (s < 12600)
        return 7200;
    //else if(s <= 330 * 60)
    else if (s <= 19800)
        return s - 5400; //90 * 60
    else
        return -2;
}

inline
int normal_time_to_hhmmss(int ss) {
    if (ss < 0)
        return -1;
    else if (ss < 120 * 60)
        return s_to_hhmmss(hhmmss_to_s(93000) + ss);
    else
        return s_to_hhmmss(hhmmss_to_s(93000) + ss + 90 * 60);
}

inline
int ms_to_normal_mstime(int ms) {
    return hhmmss_to_normal_time(ms_to_hhmmss(ms));
}

inline
int normal_mstime_to_ms(int nms) {
    if (nms < 0) return -1;
    if (hhmmss_to_ms(93000) + nms <= hhmmss_to_ms(113000)) return hhmmss_to_ms(93000) + nms;
    return hhmmss_to_ms(130000) + nms - 3600 * 1000;
}

//转换成毫秒整数
inline
int hhmmssxxx_to_normal_time(int hhmmssxxx) {
    int t1 = hhmmss_to_normal_time(hhmmssxxx / 1000);
    if (t1 < 0) return t1;
    return t1 * 1000 + hhmmssxxx % 1000;
}

inline
int delta_time(int hhmmss_from, int hhmmss_to) {
    return hhmmss_to / 10000 * 3600 - hhmmss_from / 10000 * 3600
        + hhmmss_to % 10000 / 100 * 60 - hhmmss_from % 10000 / 100 * 60
        + hhmmss_to % 100 - hhmmss_from % 100;
}

inline
int getIdx(int curTime) {
    int idx = hhmmss_to_normal_time(curTime);
    if (idx < 0) return idx;
    return idx / 60;
}


inline
int current(char *buf, int max, const char *format) {
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);
    strftime(buf, max, format, tmp);
    return 0;
}

inline
int current() {
    char buf[128];
    current(buf, 128, "%H%M%S");
    return atoi(buf);
}

inline
int today() {
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);

    char buf[128];
    strftime(buf, 128, "%Y%m%d", tmp);
    return atoi(buf);
}
