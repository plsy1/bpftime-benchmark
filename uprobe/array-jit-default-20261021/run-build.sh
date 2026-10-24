#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
RESULTS=/home/jetson/src/results/uprobe/array-jit-default-20261021
BUILD="$ROOT/build-jit"
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/build-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/build-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/build-started-at.txt"
cmake -S "$ROOT" -B "$BUILD" >"$RESULTS/reconfigure.log" 2>&1
cmake --build "$BUILD" --target bpftime_runtime_tests bpftime-agent bpftime-syscall-server bpftimetool -j2 >"$RESULTS/build.log" 2>&1
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-default]' >"$RESULTS/default-tests.log" 2>&1
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    >"$RESULTS/tests.log" 2>&1
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' \
    >"$RESULTS/host-contract.log" 2>&1
printf 'Default array JIT build and correctness checks completed.\n'
