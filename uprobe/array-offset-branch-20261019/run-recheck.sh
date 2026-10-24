#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
RESULTS=/home/jetson/src/results/uprobe/array-offset-branch-20261019
BASE=/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors
CANDIDATE=/tmp/bpftime-array-offset-branch-20261019.z1JKDF/branch
finish()
{
    status=$?
    printf '%s\n' "$status" >"$RESULTS/recheck-exit-status.txt"
    date --iso-8601=seconds >"$RESULTS/recheck-finished-at.txt"
    exit "$status"
}
trap finish EXIT
cd "$ROOT"
test ! -e /dev/shm/bpftime_maps_shm
test "$(git rev-parse HEAD)" = fefe2f6edf276e1b2957711203b5022dd1be8d7c
date --iso-8601=seconds >"$RESULTS/recheck-started-at.txt"
for file in runtime/src/bpf_map/offset_ptr_access.hpp \
    runtime/src/handler/map_handler.hpp runtime/unit-test/maps/test_per_cpu_array.cpp; do
    cmp "$file" "$RESULTS/source/$file"
done
git diff --exit-code -- runtime/src/bpf_map/userspace/array_map.cpp
sha256sum --check "$RESULTS/full10/runtime-sha256.txt"
sha256sum --check "$RESULTS/preserved-user-files.sha256"
python3 -u "$RESULTS/recheck-driver.py" \
    --baseline "$BASE" --candidate "$CANDIDATE" \
    --output "$RESULTS/precommit-recheck-ab6" --suite array \
    --runs 6 --iterations 100000 --warmup 3000
sha256sum --check "$RESULTS/full10/runtime-sha256.txt"
sha256sum --check "$RESULTS/preserved-user-files.sha256"
printf 'Precommit six-round recheck complete; inspect paired results before deciding.\n'
