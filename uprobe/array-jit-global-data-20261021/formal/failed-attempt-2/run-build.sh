#!/usr/bin/env bash
# Formal source + LLVM 16 build for d80e8de (global data in array JIT).
# Options match the archived array-jit-default-delete-fix-20261021 build-jit
# (checked below); workloads are rebuilt from this tree with the daily no-BTF
# procedure; correctness uses the complete daily test filter.
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/array-jit-global-data-20261021/formal
ROOT=/home/jetson/src/bpftime-global-data-d80e8de-20261021
BUILD="$ROOT/build-jit"
AUTOMATION=/home/jetson/src/bpftime-jetson-daily
COMMIT=d80e8de077b4447819d5ba9ee24102e37333ebc8
OLD_CACHE=/home/jetson/src/bpftime-percpu-array-jit-20261020/build-jit/CMakeCache.txt
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/build-exit-status.txt"; date --iso-8601=seconds >"$OUT/build-finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/build-started-at.txt"

test ! -e "$ROOT"
git -C "$AUTOMATION" worktree add --detach "$ROOT" "$COMMIT" >"$OUT/worktree.log" 2>&1
git -C "$ROOT" submodule sync --recursive >>"$OUT/worktree.log" 2>&1
git -C "$ROOT" submodule update --init --recursive --jobs 6 >>"$OUT/worktree.log" 2>&1

cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-16/cmake \
    -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=ON \
    -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF \
    -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF \
    -DBUILD_BPFTIME_DAEMON=OFF -DBUILD_LLVM_AOT_CLI=OFF \
    -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO >"$OUT/configure.log" 2>&1
options() {
    grep -E '^(BPFTIME_[A-Z_]+|ENABLE_[A-Z_]+|SPDLOG_ACTIVE_LEVEL|CMAKE_BUILD_TYPE|LLVM_DIR|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|CMAKE_C_FLAGS|CMAKE_CXX_FLAGS|BUILD_[A-Z_]+|TEST_LCOV)[:=]' "$1" \
        | grep -v ADVANCED | sed -E 's/^([A-Za-z_]+):[A-Z]+=/\1=/' | sort
}
options "$BUILD/CMakeCache.txt" >"$OUT/cmake-options.txt"
options "$OLD_CACHE" >"$OUT/cmake-options-previous.txt"
diff "$OUT/cmake-options-previous.txt" "$OUT/cmake-options.txt" >"$OUT/cmake-options.diff"

cmake --build "$BUILD" --parallel 6 --target bpftime-agent bpftime-syscall-server \
    bpftime_runtime_tests bpftimetool simple-benchmark-with-embed-ebpf-calling >"$OUT/build.log" 2>&1

# Benchmark workloads exactly as the daily tracker builds them (no-BTF vmlinux).
VMLINUX=$(python3 -I - "$AUTOMATION" "$ROOT" "$BUILD" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / 'tools/jetson-upstream'))
import tracker
config, _ = tracker.load_config(Path(sys.argv[1]) / 'tools/jetson-upstream/config.json')
print(tracker.benchmark_vmlinux(config, Path(sys.argv[2]), Path(sys.argv[3])))
PY
)
VMLINUX=${VMLINUX##*$'\n'}  # tracker.run echoes its git commands first
test -f "$VMLINUX"
printf '%s\n' "$VMLINUX" >"$OUT/vmlinux-path.txt"
make -C "$ROOT/benchmark" test >"$OUT/build-workloads.log" 2>&1
make -C "$ROOT/benchmark/uprobe" CLANG=/usr/bin/clang-15 CC=/usr/bin/gcc-13 \
    "VMLINUX=$VMLINUX" uprobe >>"$OUT/build-workloads.log" 2>&1

# Consuming array workloads: same commands as the archived build-diagnostic.sh.
O="$ROOT/benchmark/uprobe/.output"
for kind in ordinary percpu; do
    if [[ $kind == ordinary ]]; then dir=array-jit-poc; obj=array_jit_poc; loader=array_jit_poc_loader.c
    else dir=percpu-array-jit; obj=percpu_array_jit; loader=percpu_array_jit_loader.c; fi
    mkdir -p "$O/$dir"
    /usr/bin/clang-15 -g -O2 -target bpf -D__TARGET_ARCH_arm64 \
        -I"$ROOT/third_party/vmlinux/arm64" -I"$O" -I"$ROOT/third_party/libbpf/include/uapi" \
        -c "$ROOT/benchmark/uprobe/diagnostics/$obj.bpf.c" -o "$O/$dir/$obj.bpf.o"
    "$O/bpftool/bootstrap/bpftool" gen skeleton "$O/$dir/$obj.bpf.o" >"$O/$dir/$obj.skel.h"
    /usr/bin/gcc-13 -g -O2 -I"$O/$dir" -I"$O" "$ROOT/benchmark/uprobe/diagnostics/$loader" \
        "$O/libbpf.a" -lelf -lz -o "$O/$dir/loader"
    /usr/bin/gcc-13 -g -O2 -fno-ipa-icf -rdynamic \
        "$ROOT/benchmark/uprobe/diagnostics/array_jit_poc_victim.c" -o "$O/$dir/victim"
done >>"$OUT/build-workloads.log" 2>&1
# libbpf-style globals variants (.rodata setting + .bss counter), own victims.
for kind in ordinary percpu; do
    if [[ $kind == ordinary ]]; then dir=array-jit-globals; skel=array_jit_poc; loader=array_jit_poc_loader.c; extra=()
    else dir=percpu-array-jit-globals; skel=percpu_array_jit; loader=percpu_array_jit_loader.c
        extra=(-DBPFTIME_ARRAY_JIT_MAP_TYPE=BPF_MAP_TYPE_PERCPU_ARRAY); fi
    mkdir -p "$O/$dir"
    /usr/bin/clang-15 -g -O2 -target bpf -D__TARGET_ARCH_arm64 -DBPFTIME_ARRAY_JIT_GLOBALS=1 "${extra[@]}" \
        "-DBPFTIME_ARRAY_JIT_PATH=\"uprobe/benchmark/uprobe/.output/$dir/victim:\"" \
        -I"$ROOT/third_party/vmlinux/arm64" -I"$O" -I"$ROOT/third_party/libbpf/include/uapi" \
        -c "$ROOT/benchmark/uprobe/diagnostics/array_jit_poc.bpf.c" -o "$O/$dir/$skel.bpf.o"
    "$O/bpftool/bootstrap/bpftool" gen skeleton "$O/$dir/$skel.bpf.o" name "${skel}_bpf" >"$O/$dir/$skel.skel.h"
    /usr/bin/gcc-13 -g -O2 -I"$O/$dir" -I"$O" "$ROOT/benchmark/uprobe/diagnostics/$loader" \
        "$O/libbpf.a" -lelf -lz -o "$O/$dir/loader"
    /usr/bin/gcc-13 -g -O2 -fno-ipa-icf -rdynamic \
        "$ROOT/benchmark/uprobe/diagnostics/array_jit_poc_victim.c" -o "$O/$dir/victim"
done >>"$OUT/build-workloads.log" 2>&1

FILTER=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["build"]["test_filter"])' \
    "$AUTOMATION/tools/jetson-upstream/config.json")
printf '%s\n' "$FILTER" >"$OUT/test-filter.txt"
(cd "$ROOT" && env -u BPFTIME_ARRAY_JIT_INLINE timeout 600 "$BUILD/runtime/unit-test/bpftime_runtime_tests" "$FILTER") \
    >"$OUT/tests.log" 2>&1

# Freeze measured runtime artifacts; later steps use only these copies.
A="$OUT/artifacts"
for item in runtime/agent/libbpftime-agent.so runtime/syscall-server/libbpftime-syscall-server.so \
        tools/bpftimetool/bpftimetool benchmark/simple-benchmark-with-embed-ebpf-calling; do
    mkdir -p "$A/$(dirname "$item")"
    cp "$BUILD/$item" "$A/$item"
done
sha256sum "$A"/runtime/agent/libbpftime-agent.so "$A"/runtime/syscall-server/libbpftime-syscall-server.so \
    "$A"/tools/bpftimetool/bpftimetool "$A"/benchmark/simple-benchmark-with-embed-ebpf-calling \
    "$ROOT"/benchmark/test "$ROOT"/benchmark/uprobe/uprobe "$ROOT"/benchmark/uprobe/.output/uprobe.bpf.o \
    "$O"/array-jit-poc/{loader,victim,array_jit_poc.bpf.o} \
    "$O"/percpu-array-jit/{loader,victim,percpu_array_jit.bpf.o} \
    "$O"/array-jit-globals/{loader,victim,array_jit_poc.bpf.o} \
    "$O"/percpu-array-jit-globals/{loader,victim,percpu_array_jit.bpf.o} >"$OUT/artifact-sha256.txt"

{
    echo "commit: $(git -C "$ROOT" rev-parse HEAD)"
    echo "branch: codex/uprobe-official-177a1dc465ff (detached worktree $ROOT)"
    echo "official base: 177a1dc465ff6e4122e3bda9b280018c9a324b80"
    echo "vm/llvm-jit: $(git -C "$ROOT/vm/llvm-jit" rev-parse HEAD)"
    echo "tracked status (empty = clean):"
    git -C "$ROOT" status --short --untracked-files=no
    git -C "$ROOT/vm/llvm-jit" status --short
} >"$OUT/source-state.txt"
test -z "$(git -C "$ROOT" status --short --untracked-files=no)"
test -z "$(git -C "$ROOT/vm/llvm-jit" status --short)"
grep -q 'All tests passed' "$OUT/tests.log"
