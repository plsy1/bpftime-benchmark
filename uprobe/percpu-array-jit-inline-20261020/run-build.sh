#!/usr/bin/env bash
set -euo pipefail
SOURCE=/home/jetson/src/bpftime-percpu-array-jit-20261020
RESULTS=/home/jetson/src/results/uprobe/percpu-array-jit-inline-20261020
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
    -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 >"$RESULTS/configure.log" 2>&1
cmake --build "$SOURCE/build-jit" --target bpftime_runtime_tests bpftimetool -j2 >"$RESULTS/build.log" 2>&1
# The second target may refresh the LLVM overlay and static runtime. Relink
# their consumers directly, with no extra unconditional libbpf install.
for target in runtime/agent/libbpftime-agent.so runtime/syscall-server/libbpftime-syscall-server.so runtime/unit-test/bpftime_runtime_tests; do
    case "$target" in
        runtime/agent/*) rules=runtime/agent/CMakeFiles/bpftime-agent.dir/build.make ;;
        runtime/syscall-server/*) rules=runtime/syscall-server/CMakeFiles/bpftime-syscall-server.dir/build.make ;;
        *) rules=runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/build.make ;;
    esac
    gmake --no-print-directory -C "$SOURCE/build-jit" -f "$rules" "$target" >>"$RESULTS/build.log" 2>&1
done
timeout 180 "$SOURCE/build-jit/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    >"$RESULTS/tests.log" 2>&1
printf 'Per-CPU JIT build and correctness tests completed.\n'
