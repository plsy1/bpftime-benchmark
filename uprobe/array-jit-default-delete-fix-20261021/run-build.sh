#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
OUT=/home/jetson/src/results/uprobe/array-jit-default-delete-fix-20261021
BUILD="$ROOT/build-jit"
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/build-exit-status.txt"; date --iso-8601=seconds >"$OUT/build-finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/build-started-at.txt"
python3 "$OUT/archive-source.py"
make_target() {
    gmake --no-print-directory -C "$BUILD" -f "$1" "${1%/build.make}/depend" >>"$OUT/build.log" 2>&1
    gmake --no-print-directory -C "$BUILD" -f "$1" "$2" -j2 >>"$OUT/build.log" 2>&1
}
make_target runtime/CMakeFiles/runtime.dir/build.make runtime/libruntime.a
make_target runtime/agent/CMakeFiles/bpftime-agent.dir/build.make runtime/agent/libbpftime-agent.so
make_target runtime/syscall-server/CMakeFiles/bpftime-syscall-server.dir/build.make runtime/syscall-server/libbpftime-syscall-server.so
make_target runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/build.make runtime/unit-test/bpftime_runtime_tests
make_target tools/bpftimetool/CMakeFiles/bpftimetool.dir/build.make tools/bpftimetool/bpftimetool
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' >"$OUT/tests.log" 2>&1
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' >"$OUT/host-contract.log" 2>&1
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" '[array-jit-default]' >"$OUT/default-tests.log" 2>&1
for item in runtime/agent/libbpftime-agent.so runtime/syscall-server/libbpftime-syscall-server.so tools/bpftimetool/bpftimetool; do
    mkdir -p "$OUT/artifacts/$(dirname "$item")"
    cp "$BUILD/$item" "$OUT/artifacts/$item"
done
sha256sum "$OUT/artifacts/runtime/agent/libbpftime-agent.so" "$OUT/artifacts/runtime/syscall-server/libbpftime-syscall-server.so" "$OUT/artifacts/tools/bpftimetool/bpftimetool" >"$OUT/artifact-sha256.txt"
