#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/percpu-array-jit-direct-cpu-20261020
BUILD=/home/jetson/src/bpftime-percpu-array-jit-20261020/build-jit
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/regression-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/regression-finished-at.txt"' EXIT
test "$(<"$RESULTS/short-exit-status.txt")" = 0
python3 -u "$RESULTS/run-regression.py" --baseline "$RESULTS/baseline-build" --candidate "$BUILD" \
    --candidate-inline --suite array --output "$RESULTS/array-regression3" --runs 3 --iterations 30000
# Ordinary hash update is known bimodal: use 12 rounds and retain both
# mean and median, rather than rejecting on a three-sample median alone.
python3 -u "$RESULTS/run-regression.py" --baseline "$RESULTS/baseline-build" --candidate "$BUILD" \
    --candidate-inline --suite hash --output "$RESULTS/hash-regression12" --runs 12 --iterations 10000
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import json,sys
r=Path(sys.argv[1])
for suite in ('array-regression3','hash-regression12'):
    for row in json.loads((r/suite/'summary.json').read_text()):
        if not row['historically_comparable']:continue
        stat='mean' if row['case']=='hash_update' else 'median'
        ratio=row['candidate'][stat]/row['baseline'][stat]
        print(suite,row['case'],stat,ratio)
        assert ratio<=1.03,(suite,row['case'],stat,ratio)
print('Non-target array/hash gates passed.')
PY
