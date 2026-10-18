#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
RESULTS=/home/jetson/src/results/uprobe/percpu-array-lookup-followup-20261018
SNAPSHOTS=/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb
finish()
{
    status=$?
    printf '%s\n' "$status" >"$RESULTS/update-reverse-exit-status.txt"
    date --iso-8601=seconds >"$RESULTS/update-reverse-finished-at.txt"
    exit "$status"
}
trap finish EXIT
cd "$ROOT"
date --iso-8601=seconds >"$RESULTS/update-reverse-started-at.txt"
# Deliberately reverse the labels: baseline is now the new inline build.
# Both executables use the identical shared-memory layout and tool protocol.
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    --baseline "$SNAPSHOTS/inline-leaf" --candidate "$SNAPSHOTS/baseline" \
    --output "$RESULTS/update-reverse-ab6" --suite array \
    --runs 6 --iterations 100000 --warmup 5000
