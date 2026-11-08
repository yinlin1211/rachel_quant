#!/bin/bash
# 编译六个可执行文件：bin/panel_tool、bin/panel_book、bin/sim_live、bin/backtest、bin/alpha101、bin/alpha_expr
# 环境：GCC 4.8.5（只支持 C++11），依赖 liblf_tick2py.so 及其 liblquant_kernel.so
set -e
cd "$(dirname "$0")"
mkdir -p bin

CXX=${CXX:-g++}
TICKLIB=${TICKLIB:-/home/intern2/pyTickAlpha}

echo "[1/6] alpha101"
$CXX -O3 -std=c++11 -fopenmp -o bin/alpha101 src/main.cpp

echo "[2/6] alpha_expr"
$CXX -O3 -std=c++11 -fopenmp -o bin/alpha_expr src/alpha_expr.cpp

echo "[3/6] backtest"
$CXX -O3 -std=c++11 -fopenmp -o bin/backtest src/backtest.cpp

echo "[4/6] panel_book"
$CXX -O2 -std=c++11 -o bin/panel_book src/panel_book.cpp \
    "$TICKLIB/liblf_tick2py.so" \
    -L"$TICKLIB" -llquant_kernel -lrt -lpthread \
    -Wl,-rpath,"$TICKLIB"

echo "[5/6] sim_live"
$CXX -O3 -std=c++11 -o bin/sim_live src/sim_live.cpp \
    "$TICKLIB/liblf_tick2py.so" \
    -L"$TICKLIB" -llquant_kernel -lrt -lpthread \
    -Wl,-rpath,"$TICKLIB"

echo "[6/6] panel_tool"
$CXX -O2 -std=c++11 -o bin/panel_tool src/panel_tool.cpp \
    "$TICKLIB/liblf_tick2py.so" \
    -L"$TICKLIB" -llquant_kernel -lrt -lpthread \
    -Wl,-rpath,"$TICKLIB"

echo "done: bin/panel_tool bin/alpha101"
