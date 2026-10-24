#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
FROZEN=/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors
RESULTS=/home/jetson/src/results/uprobe/array-lookup-reprofile-20261019
finish()
{
    status=$?
    printf '%s\n' "$status" >"$RESULTS/profiles-exit-status.txt"
    date --iso-8601=seconds >"$RESULTS/profiles-finished-at.txt"
    exit "$status"
}
trap finish EXIT
cd "$ROOT"
test "$(git rev-parse HEAD)" = fefe2f6edf276e1b2957711203b5022dd1be8d7c
git diff --exit-code -- runtime
test ! -e /dev/shm/bpftime_maps_shm
cmp "$BUILD/runtime/agent/libbpftime-agent.so" "$FROZEN/runtime/agent/libbpftime-agent.so"
cmp "$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" "$FROZEN/runtime/syscall-server/libbpftime-syscall-server.so"
mkdir "$RESULTS/profiles" "$RESULTS/artifacts"
date --iso-8601=seconds >"$RESULTS/profiles-started-at.txt"
git rev-parse HEAD >"$RESULTS/source-commit.txt"
git status --short >"$RESULTS/source-status.txt"
cp "$FROZEN/CMakeCache.txt" "$RESULTS/artifacts/"
cp --reflink=auto "$FROZEN/runtime/agent/libbpftime-agent.so" "$RESULTS/artifacts/"
cp --reflink=auto "$FROZEN/runtime/syscall-server/libbpftime-syscall-server.so" "$RESULTS/artifacts/"
cp benchmark/uprobe/diagnostics/helper_map_closure.c \
    benchmark/uprobe/diagnostics/helper_map_closure.bpf.c \
    benchmark/uprobe/diagnostics/helper_map_closure_victim.c "$RESULTS/artifacts/"
sha256sum "$FROZEN/runtime/agent/libbpftime-agent.so" \
    "$FROZEN/runtime/syscall-server/libbpftime-syscall-server.so" \
    benchmark/uprobe/.output/helper-map-closure/helper-map-closure \
    benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim \
    benchmark/uprobe/.output/helper-map-closure/helper_map_closure.bpf.o \
    >"$RESULTS/profile-artifacts-sha256.txt"
sudo -n jetson_clocks --show >"$RESULTS/jetson-clocks.txt" 2>&1
python3 - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
values={k:int((p/f'scaling_{k}_freq').read_text()) for k in ['min','max','cur']}
assert all(x==1728000 for x in values.values()),values
print('Pinned CPU baseline frequency verified:',values,flush=True)
PY
bash "$RESULTS/capture-case.sh" array_lookup 1 1728000
bash "$RESULTS/capture-case.sh" percpu_array_lookup 1 1728000
bash "$RESULTS/capture-case.sh" percpu_array_lookup 2 1572863
bash "$RESULTS/capture-case.sh" array_lookup 2 1572863
sha256sum --check "$RESULTS/profile-artifacts-sha256.txt"
git diff --exit-code -- runtime
printf 'All four delayed-window production captures completed.\n'
