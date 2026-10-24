#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
RESULTS=/home/jetson/src/results/uprobe/percpu-array-jit-inline-20261020
BUILD="$ROOT/build-jit"
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/null-guard-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/null-guard-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/null-guard-started-at.txt"
while ! test -f "$RESULTS/key-first-exit-status.txt"; do sleep 3; done
test "$(<"$RESULTS/key-first-exit-status.txt")" = 0
mkdir "$RESULTS/artifacts-before-null-guard" "$RESULTS/source-before-null-guard"
cp "$BUILD/runtime/agent/libbpftime-agent.so" "$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" "$BUILD/runtime/unit-test/bpftime_runtime_tests" "$RESULTS/artifacts-before-null-guard/"
rsync -a --relative "$ROOT/./vm/compat/llvm-vm/array-map-inline.patch" "$ROOT/./runtime/unit-test/maps/test_array_jit_poc.cpp" "$RESULTS/source-before-null-guard/"
PRIMARY=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
rsync -a --relative "$PRIMARY/./vm/compat/llvm-vm/array-map-inline.patch" "$PRIMARY/./runtime/unit-test/maps/test_array_jit_poc.cpp" "$ROOT/"
cmake -S "$ROOT" -B "$BUILD" >"$RESULTS/reconfigure-null-guard.log" 2>&1
gmake --no-print-directory -C "$BUILD" \
    -f vm/compat/llvm-vm/llvm-jit/CMakeFiles/llvmbpf_vm.dir/build.make \
    vm/compat/llvm-vm/libllvmbpf_vm.a -j2 >"$RESULTS/build-null-guard.log" 2>&1
for target in runtime/agent/libbpftime-agent.so runtime/syscall-server/libbpftime-syscall-server.so runtime/unit-test/bpftime_runtime_tests tools/bpftimetool/bpftimetool; do
    case "$target" in
        runtime/agent/*) rules=runtime/agent/CMakeFiles/bpftime-agent.dir/build.make ;;
        runtime/syscall-server/*) rules=runtime/syscall-server/CMakeFiles/bpftime-syscall-server.dir/build.make ;;
        tools/*) rules=tools/bpftimetool/CMakeFiles/bpftimetool.dir/build.make ;;
        *) rules=runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/build.make ;;
    esac
    gmake --no-print-directory -C "$BUILD" -f "$rules" "$target" -j2 >>"$RESULTS/build-null-guard.log" 2>&1
done
timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    >"$RESULTS/tests-null-guard.log" 2>&1
timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' \
    >"$RESULTS/host-contract-null-guard.log" 2>&1
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/smoke-null-guard" --runs 1 --iterations 2000 --dump-jit
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/ab6-null-guard" --runs 6 --iterations 50000
python3 -u "$RESULTS/run-ordinary.py" --output "$RESULTS/ordinary-ab3-null-guard" --runs 3 --iterations 50000
printf 'Key-first per-CPU JIT validation completed.\n'
