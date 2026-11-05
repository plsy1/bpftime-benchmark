#!/bin/bash
# build.sh <name>: compiles <name>.cpp with the per-CPU hash map sources at 94f9ba8f
set -e
W=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
S=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/review-20261105/percpu
INC="-I$W/runtime/include -I$W/runtime -I$W/runtime/src -I$W/third_party -I$W/third_party/spdlog/include -I$W/build-jetson-daily/libbpf/uapi -I$W/build-jetson-daily/libbpf -I$W/vm/include"
DEFS="-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO -DSPDLOG_COMPILED_LIB"
g++-13 -O2 -g -std=gnu++20 -fno-omit-frame-pointer -Wall -Wno-unused $DEFS $INC   "$S/$1.cpp" "$W/runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp" "$W/runtime/src/platform_utils.cpp"   "$W/build-jetson-daily/third_party/spdlog/libspdlog.a" -lpthread -lrt -o "$S/$1"
echo built $S/$1
