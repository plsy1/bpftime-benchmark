#!/usr/bin/env bash
# x86 NUC: configure + build one tree (official 177a1dc or the dev branch) with
# the Jetson baseline options, LLVM 19 instead of 16, BPF clang 19.
# Usage: run-build.sh <label> <source-root>
set -euo pipefail
LABEL=${1:?label}
ROOT=${2:?source root}
OUT=/home/y/src/bpftime-wt-results/uprobe/x86-nuc-build-regression-20261026/$LABEL
BUILD="$ROOT/build-jit"
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$OUT"
trap 'status=$?; printf "%s\n" "$status" >"$OUT/build-exit-status.txt"; date --iso-8601=seconds >"$OUT/build-finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/build-started-at.txt"
test ! -e "$BUILD"

{
    echo "commit: $(git -C "$ROOT" rev-parse HEAD)"
    echo "vm/llvm-jit: $(git -C "$ROOT/vm/llvm-jit" rev-parse HEAD)"
    echo "tracked status (empty = clean):"
    git -C "$ROOT" status --short --untracked-files=no
    git -C "$ROOT/vm/llvm-jit" status --short
} >"$OUT/source-state.txt"

cmake -S "$ROOT" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DBPFTIME_BPF_CLANG=/usr/bin/clang-19 -DDIAGNOSTIC_BPF_CLANG=/usr/bin/clang-19 -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
    -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON \
    -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=ON \
    -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF \
    -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF \
    -DBUILD_BPFTIME_DAEMON=OFF -DBUILD_LLVM_AOT_CLI=OFF \
    -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO >"$OUT/configure.log" 2>&1
grep -E '^(BPFTIME_[A-Z_]+|ENABLE_[A-Z_]+|SPDLOG_ACTIVE_LEVEL|CMAKE_BUILD_TYPE|LLVM_DIR|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|CMAKE_C_FLAGS|CMAKE_CXX_FLAGS|BUILD_[A-Z_]+|TEST_LCOV)[:=]' "$BUILD/CMakeCache.txt" \
    | grep -v ADVANCED | sed -E 's/^([A-Za-z_]+):[A-Z]+=/\1=/' | sort >"$OUT/cmake-options.txt"

cmake --build "$BUILD" --parallel 14 --target bpftime-agent bpftime-syscall-server \
    bpftime_runtime_tests bpftimetool simple-benchmark-with-embed-ebpf-calling \
    bpftime_frida_uprobe_attach_tests >"$OUT/build.log" 2>&1
