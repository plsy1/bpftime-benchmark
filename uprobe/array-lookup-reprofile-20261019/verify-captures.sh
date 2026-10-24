#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/array-lookup-reprofile-20261019
cd /home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
sha256sum --check "$RESULTS/profile-artifacts-sha256.txt"
git diff --exit-code -- runtime
for item in array_lookup-1 array_lookup-2 percpu_array_lookup-1 percpu_array_lookup-2; do
    test -s "$RESULTS/profiles/$item/capture.sqlite"
    case_name=${item%-*}
    rg -q "^$case_name," "$RESULTS/profiles/$item/victim.log"
done
printf '0\n' >"$RESULTS/resume-exit-status.txt"
date --iso-8601=seconds >"$RESULTS/resume-finished-at.txt"
