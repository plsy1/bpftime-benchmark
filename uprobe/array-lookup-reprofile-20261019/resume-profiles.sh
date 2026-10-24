#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/array-lookup-reprofile-20261019
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/resume-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/resume-finished-at.txt"' EXIT
sudo -n nsys export --type sqlite --output="$RESULTS/profiles/percpu_array_lookup-1/capture.sqlite" "$RESULTS/profiles/percpu_array_lookup-1/capture.nsys-rep" >"$RESULTS/profiles/percpu_array_lookup-1/export.log" 2>&1
bash "$RESULTS/capture-case.sh" percpu_array_lookup 2 1572863
bash "$RESULTS/capture-case.sh" array_lookup 2 1572863
cd /home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
sha256sum --check "$RESULTS/profile-artifacts-sha256.txt"
git diff --exit-code -- runtime
