#!/bin/bash
# build-bt.sh SRC_TREE OUT: r1bt against the hash map header of SRC_TREE.
set -eu
W=$1
g++ -std=c++20 -O2 -pthread -DSPDLOG_HEADER_ONLY -I $W/runtime/src -I $W/runtime/include \
  -I $W/third_party/spdlog/include r1bt.cpp -o $2 -lrt 2>&1 | grep -E "error" || true
test -x $2
