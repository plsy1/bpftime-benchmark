#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CODE=/home/y1/src/bpftime-offical-no-btf
BUILD=$CODE/build-map-path-x64-20260802
LLVM=/home/y1/src/llvm-project-15.0.7

{
    echo "captured_at=$(date --iso-8601=seconds)"
    echo
    echo '[host]'
    uname -a
    sed -n '1,12p' /etc/os-release
    lscpu
    echo "microcode=$(sed -n 's/^microcode[[:space:]]*: //p' /proc/cpuinfo | head -1)"
    echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid)"
    echo "kptr_restrict=$(cat /proc/sys/kernel/kptr_restrict)"
    perf --version
    echo
    echo '[code]'
    git -C "$CODE" status --short --branch
    echo "commit=$(git -C "$CODE" rev-parse HEAD)"
    echo "describe=$(git -C "$CODE" describe --always --dirty)"
    git -C "$CODE" submodule status
    echo
    echo '[results]'
    git -C "$ROOT" status --short --branch
    echo "commit=$(git -C "$ROOT" rev-parse HEAD)"
    echo
    echo '[toolchain]'
    /usr/bin/gcc-13 --version | head -1
    /usr/bin/g++-13 --version | head -1
    "$LLVM/build-kmr-gcc12/bin/clang" --version | head -3
    echo "llvm_source_commit=$(git -C "$LLVM" rev-parse HEAD)"
    echo "llvm_targets=$(sed -n 's/^LLVM_TARGETS_TO_BUILD:STRING=//p' "$LLVM/build-kmr-gcc12/CMakeCache.txt")"
    echo "llvm_build_c_compiler=$(sed -n 's/^CMAKE_C_COMPILER:STRING=//p' "$LLVM/build-kmr-gcc12/CMakeCache.txt")"
    echo "boost_version=$(sed -n 's/^#define BOOST_LIB_VERSION //p' /home/y1/src/.toolchains/boost-1.83/usr/include/boost/version.hpp)"
    echo "boost_root=/home/y1/src/.toolchains/boost-1.83/usr"
    cmake --version | head -1
    ninja --version
    echo
    echo '[cmake-cache]'
    grep -E '^(BPFTIME_LLVM_JIT|BPFTIME_ENABLE_LTO|ENABLE_PROBE_(READ|WRITE)_CHECK|CMAKE_BUILD_TYPE|CMAKE_(C|CXX)_COMPILER:|Boost_DIR|Boost_INCLUDE_DIR|LLVM_DIR|DIAGNOSTIC_BPF_CLANG):' "$BUILD/CMakeCache.txt" | sort
    echo "diagnostic_callers_ipo=$(grep -n 'INTERPROCEDURAL_OPTIMIZATION FALSE' "$CODE/benchmark/CMakeLists.txt" | tr '\n' ';')"
    runtime_command=$(ninja -C "$BUILD" -t commands | awk '/runtime\/CMakeFiles\/runtime.dir\/src\/bpf_helper.cpp.o/ && !found { line=$0; found=1 } END { print line }')
    diagnostic_command=$(ninja -C "$BUILD" -t commands | awk '/array_map_path_layers.cpp.o/ && !found { line=$0; found=1 } END { print line }')
    echo "runtime_compile_has_flto=$(grep -q -- '-flto' <<<"$runtime_command" && echo yes || echo no)"
    echo "diagnostic_compile_has_flto=$(grep -q -- '-flto' <<<"$diagnostic_command" && echo yes || echo no)"
    echo "runtime_object_compiler=$(readelf -p .comment "$BUILD/runtime/CMakeFiles/runtime.dir/src/bpf_helper.cpp.o" 2>/dev/null | sed -n 's/.*GCC:/GCC:/p' | head -1)"
    echo "ubpf_compat_object_compiler=$(readelf -p .comment "$BUILD/vm/compat/ubpf-vm/ubpf/vm/CMakeFiles/ubpf.dir/ubpf_vm.c.o" 2>/dev/null | sed -n 's/.*GCC:/GCC:/p' | head -1)"
    echo
    echo '[measurement-window]'
    cat "$ROOT/system-state-during.txt"
    echo "governor_after=$(cat /sys/devices/system/cpu/cpufreq/policy5/scaling_governor)"
    echo "sibling_online_after=$(cat /sys/devices/system/cpu/cpu11/online)"
    echo "turbo_no_turbo_after=$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)"
    echo
    echo '[parameters]'
    echo 'jit_array=100000 invocations x 1000 helpers x 3 rounds; warmup 1000 invocations x 1000 helpers; perf denominator 301000000 helpers'
    echo 'jit_hash=20000 invocations x 1000 helpers x 3 rounds; warmup 1000 invocations x 1000 helpers; perf denominator 61000000 helpers'
    echo 'direct_array=100000000 ops x 3 rounds; warmup 1000000 ops; perf denominator 301000000 ops'
    echo 'direct_hash=10000000 ops x 3 rounds; warmup 1000000 ops; perf denominator 31000000 ops'
    echo 'perf_events=task-clock,cycles,instructions,branches,branch-misses,cache-references,cache-misses'
} > "$ROOT/environment.txt"
