#!/bin/bash
# 编译两个可执行文件：bin/panel_tool、bin/alpha101
# 环境：GCC 4.8.5（只支持 C++11），依赖 liblf_tick2py.so 及其 liblquant_kernel.so
set -e
cd "$(dirname "$0")"
mkdir -p bin

CXX=${CXX:-g++}
TICKLIB=${TICKLIB:-/home/intern2/pyTickAlpha}

echo "[1/3] alpha101"
$CXX -O3 -std=c++11 -fopenmp -o bin/alpha101 src/main.cpp

echo "[2/3] alpha_expr"
$CXX -O3 -std=c++11 -fopenmp -o bin/alpha_expr src/alpha_expr.cpp

echo "[3/3] panel_tool"
$CXX -O2 -std=c++11 -o bin/panel_tool src/panel_tool.cpp \
    "$TICKLIB/liblf_tick2py.so" \
    -L"$TICKLIB" -llquant_kernel -lrt -lpthread \
    -Wl,-rpath,"$TICKLIB"

echo "done: bin/panel_tool bin/alpha101"
