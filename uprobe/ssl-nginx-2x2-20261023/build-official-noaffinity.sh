#!/usr/bin/env bash
# Official 177a1dc plus only the perf output affinity removal (20870a03's
# bpf_helper.cpp change), built with the options of the official reference
# build in consuming-official-vs-optimized-20261021. Also builds sslsniff.
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
ROOT=/home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023
BUILD="$ROOT/build-jit"
trap 'status=$?; printf "%s\n" "$status" >"$OUT/build-exit-status.txt"' EXIT
test ! -e "$ROOT"
git -C /home/jetson/src/bpftime-jetson-daily worktree add --detach "$ROOT" 177a1dc465ff6e4122e3bda9b280018c9a324b80 >"$OUT/worktree.log" 2>&1
git -C "$ROOT" submodule update --init --recursive --jobs 6 >>"$OUT/worktree.log" 2>&1
git -C "$ROOT" apply "$OUT/no-affinity.patch"
git -C "$ROOT" diff >"$OUT/official-noaffinity.diff"
cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-16/cmake \
    -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=ON \
    -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF \
    -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF \
    -DBUILD_BPFTIME_DAEMON=OFF -DBUILD_LLVM_AOT_CLI=OFF \
    -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO >"$OUT/configure.log" 2>&1
cmake --build "$BUILD" --parallel 6 --target bpftime-agent bpftime-syscall-server bpftimetool >"$OUT/build.log" 2>&1
make -C "$ROOT/example/tracing/sslsniff" CLANG=/usr/bin/clang-15 -j6 >"$OUT/sslsniff-build.log" 2>&1
