#!/usr/bin/env bash
# Development build of the global-data change (uncommitted working tree),
# LLVM 16 with the baseline's options, then the new tests and the full daily filter.
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/array-jit-global-data-20261021/dev
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
BUILD="$ROOT/build-jit"
mkdir -p "$OUT"
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/exit-status.txt"; date --iso-8601=seconds >"$OUT/finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"
git -C "$ROOT" diff >"$OUT/working-tree.diff"
cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-16/cmake \
    -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=ON \
    -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF \
    -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF \
    -DBUILD_BPFTIME_DAEMON=OFF -DBUILD_LLVM_AOT_CLI=OFF \
    -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO >"$OUT/configure.log" 2>&1
cmake --build "$BUILD" --parallel 6 --target bpftime_runtime_tests >"$OUT/build.log" 2>&1
T="$BUILD/runtime/unit-test/bpftime_runtime_tests"
(cd "$ROOT" && timeout 300 "$T" '[array-jit-global]') >"$OUT/global-tests.log" 2>&1 || echo "global tests failed: $?" >>"$OUT/failures.txt"
FILTER=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["build"]["test_filter"])' \
    /home/jetson/src/bpftime-jetson-daily/tools/jetson-upstream/config.json)
(cd "$ROOT" && timeout 600 "$T" "$FILTER") >"$OUT/daily-filter-tests.log" 2>&1 || echo "daily filter failed: $?" >>"$OUT/failures.txt"
test ! -e "$OUT/failures.txt"
