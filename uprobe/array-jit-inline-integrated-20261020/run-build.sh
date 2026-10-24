#!/usr/bin/env bash
set -euo pipefail
SOURCE=/home/jetson/src/bpftime-array-jit-integrated-20261020
RESULTS=/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/build-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/build-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/build-started-at.txt"
cmake -S "$SOURCE" -B "$SOURCE/build-jit" \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBPFTIME_ENABLE_LTO=ON \
    -DBPFTIME_ENABLE_UNIT_TESTING=ON -DBPFTIME_BUILD_WITH_LIBBPF=ON \
    -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON \
    -DBPFTIME_ENABLE_MAP_PRODUCTION_AB=OFF -DENABLE_PROBE_READ_CHECK=OFF \
    -DENABLE_PROBE_WRITE_CHECK=OFF -DBPFTIME_ENABLE_MPK=OFF \
    -DBPFTIME_ENABLE_TRAP_UPROBE=OFF -DBPFTIME_ENABLE_CUDA_ATTACH=OFF \
    -DLLVM_DIR=/usr/lib/llvm-16/cmake -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO \
    -DBPFTIME_ENABLE_CCACHE=OFF -DBUILD_BPFTIME_DAEMON=OFF \
    -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 \
    >"$RESULTS/configure.log" 2>&1
# The test target already depends on both agent and syscall-server. One graph
# traversal avoids rerunning libbpf's always-install step for each component.
cmake --build "$SOURCE/build-jit" --target bpftime_runtime_tests -j2 >"$RESULTS/build.log" 2>&1
if ! test -x "$SOURCE/build-jit/tools/bpftimetool/bpftimetool"; then
    cmake --build "$SOURCE/build-jit" --target bpftimetool -j2 >>"$RESULTS/build.log" 2>&1
fi
timeout 180 "$SOURCE/build-jit/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    >"$RESULTS/tests.log" 2>&1
printf 'Integrated build and selected correctness tests completed.\n'
