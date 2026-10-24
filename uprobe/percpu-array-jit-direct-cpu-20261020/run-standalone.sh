#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/percpu-array-jit-direct-cpu-20261020
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/standalone-exit-status.txt"' EXIT
# Do not compile/link while timed A/B arms are running.
while ! test -f "$RESULTS/regression-exit-status.txt"; do sleep 3; done
test "$(<"$RESULTS/regression-exit-status.txt")" = 0
taskset -c 0 /usr/bin/g++-13 -std=c++20 -O2 \
    -I"$ROOT/vm/llvm-jit/include" -I"$ROOT/vm/llvm-jit/src" -I"$ROOT/vm/compat/include" \
    "$RESULTS/standalone-vm.cpp" "$ROOT/build-jit/vm/compat/llvm-vm/libllvmbpf_vm.a" \
    "$ROOT/build-jit/third_party/spdlog/libspdlog.a" -lLLVM-16 -lpthread -ldl -lz -ltinfo \
    -o "$RESULTS/standalone-vm" >"$RESULTS/build-standalone.log" 2>&1
taskset -c 0 "$RESULTS/standalone-vm" >"$RESULTS/standalone.log" 2>&1
