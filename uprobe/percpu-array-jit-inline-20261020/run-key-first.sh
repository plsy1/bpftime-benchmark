#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
RESULTS=/home/jetson/src/results/uprobe/percpu-array-jit-inline-20261020
BUILD="$ROOT/build-jit"
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/key-first-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/key-first-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/key-first-started-at.txt"
cmake -S "$ROOT" -B "$BUILD" >"$RESULTS/reconfigure-key-first.log" 2>&1
gmake --no-print-directory -C "$BUILD" \
    -f vm/compat/llvm-vm/llvm-jit/CMakeFiles/llvmbpf_vm.dir/build.make \
    vm/compat/llvm-vm/libllvmbpf_vm.a -j2 >"$RESULTS/build-key-first.log" 2>&1
for target in runtime/agent/libbpftime-agent.so runtime/syscall-server/libbpftime-syscall-server.so runtime/unit-test/bpftime_runtime_tests tools/bpftimetool/bpftimetool; do
    case "$target" in
        runtime/agent/*) rules=runtime/agent/CMakeFiles/bpftime-agent.dir/build.make ;;
        runtime/syscall-server/*) rules=runtime/syscall-server/CMakeFiles/bpftime-syscall-server.dir/build.make ;;
        tools/*) rules=tools/bpftimetool/CMakeFiles/bpftimetool.dir/build.make ;;
        *) rules=runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/build.make ;;
    esac
    gmake --no-print-directory -C "$BUILD" -f "$rules" "$target" -j2 >>"$RESULTS/build-key-first.log" 2>&1
done
timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    >"$RESULTS/tests-key-first.log" 2>&1
timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' \
    >"$RESULTS/host-contract-key-first.log" 2>&1
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/smoke-key-first" --runs 1 --iterations 2000 --dump-jit
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/ab6-key-first" --runs 6 --iterations 50000
python3 -u "$RESULTS/run-ordinary.py" --output "$RESULTS/ordinary-ab3-key-first" --runs 3 --iterations 50000
printf 'Key-first per-CPU JIT validation completed.\n'
