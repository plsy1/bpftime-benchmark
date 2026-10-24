#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
RESULTS=/home/jetson/src/results/uprobe/percpu-array-address-dependency-20261019
finish()
{
    status=$?
    printf '%s\n' "$status" >"$RESULTS/restore-exit-status.txt"
    date --iso-8601=seconds >"$RESULTS/restore-finished-at.txt"
    exit "$status"
}
trap finish EXIT
cd "$ROOT"
test "$(git rev-parse HEAD)" = fefe2f6edf276e1b2957711203b5022dd1be8d7c
git diff --exit-code -- runtime
date --iso-8601=seconds >"$RESULTS/restore-started-at.txt"
SNAPSHOTS=/tmp/bpftime-percpu-array-address-20261019.S1mHZP
test ! -e /dev/shm/bpftime_maps_shm
for artifact in runtime/agent/libbpftime-agent.so \
    runtime/syscall-server/libbpftime-syscall-server.so \
    tools/bpftimetool/bpftimetool; do
    cp "$SNAPSHOTS/baseline/$artifact" "$BUILD/$artifact"
    cmp "$SNAPSHOTS/baseline/$artifact" "$BUILD/$artifact"
done
printf 'Accepted frozen production binaries restored before rebuild.\n'
printf 'Rebuilding accepted baseline after rejecting the cold-path candidate.\n'
cmake --build "$BUILD" --target bpftime-agent bpftime-syscall-server \
    bpftimetool bpftime_runtime_tests -j2 >"$RESULTS/restore-build.log" 2>&1
printf 'Rechecking accepted baseline array/hash tests.\n'
"$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    2>&1 | tee "$RESULTS/restore-tests.log"
git diff --exit-code -- runtime
sha256sum "$BUILD/runtime/agent/libbpftime-agent.so" \
    "$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" \
    >"$RESULTS/restored-runtime-sha256.txt"
printf 'Accepted baseline restored and correctness checks passed.\n'
