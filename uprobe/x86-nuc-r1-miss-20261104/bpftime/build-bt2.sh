#!/bin/bash
# build-bt2.sh HEADER_ROOT OUT: r1bt with the hash map header under HEADER_ROOT
# (runtime/src/bpf_map/bpftime_hash_map.hpp), the rest from the d5279469 tree.
set -eu
W=$HOME/src/bpftime-wt-uprobe-177a1dc
g++ -std=c++20 -O2 -pthread -DSPDLOG_HEADER_ONLY -I $1/runtime/src -I $W/runtime/src -I $W/runtime/include \
  -I $W/third_party/spdlog/include r1bt.cpp -o $2 -lrt 2>&1 | grep -E "error" || true
test -x $2
