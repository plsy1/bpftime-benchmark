#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
RESULTS=/home/jetson/src/results/uprobe/percpu-array-lookup-followup-20261018
SNAPSHOTS=/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb
finish()
{
    status=$?
    printf '%s\n' "$status" >"$RESULTS/update-recheck-exit-status.txt"
    date --iso-8601=seconds >"$RESULTS/update-recheck-finished-at.txt"
    exit "$status"
}
trap finish EXIT
cd "$ROOT"
date --iso-8601=seconds >"$RESULTS/update-recheck-started-at.txt"
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    --baseline "$SNAPSHOTS/baseline" --candidate "$SNAPSHOTS/inline-leaf" \
    --output "$RESULTS/update-recheck-ab6" --suite array \
    --runs 6 --iterations 100000 --warmup 5000
