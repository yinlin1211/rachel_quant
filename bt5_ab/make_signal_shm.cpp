// bt5 信号共享内存写入器
// 用法: make_signal_shm <shm_name> <capacity> [signal.csv]
// signal.csv 每行: didx,tidx,cidx,score
// 不传 csv 时只创建一个空信号段，用于冒烟测试。
#include "ShmmdStruct.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

struct trade_signal {
    long didx;
    long tidx;
    long cidx;
    float score;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <shm_name> <capacity> [signal.csv]\n", argv[0]);
        return 1;
    }
    std::string shmName = argv[1];
    size_t cap = (size_t)atol(argv[2]);
    if (cap == 0) cap = 1;

    ShmStruct<trade_signal> writer;
    if (!writer.init(shmName, true, cap)) {
        fprintf(stderr, "init shm failed: %s\n", shmName.c_str());
        return 2;
    }
    fprintf(stderr, "shm created: %s capacity=%zu sizeof(trade_signal)=%zu\n",
            shmName.c_str(), cap, sizeof(trade_signal));

    if (argc >= 4) {
        std::ifstream in(argv[3]);
        if (!in) { fprintf(stderr, "cannot open %s\n", argv[3]); return 3; }
        std::string line;
        std::vector<trade_signal*> batch;
        std::vector<trade_signal> store;
        store.reserve(4096);
        long n = 0;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            trade_signal s;
            if (sscanf(line.c_str(), "%ld,%ld,%ld,%f", &s.didx, &s.tidx, &s.cidx, &s.score) != 4)
                continue;
            store.push_back(s);
            ++n;
        }
        fprintf(stderr, "read %ld signals\n", n);
        for (size_t i = 0; i < store.size(); ++i) batch.push_back(&store[i]);
        writer.pushData(batch);
    }

    fprintf(stderr, "index now = %d\n", writer.getIndex());
    return 0;
}
