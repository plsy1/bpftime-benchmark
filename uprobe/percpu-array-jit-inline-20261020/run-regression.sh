#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
RESULTS=/home/jetson/src/results/uprobe/percpu-array-jit-inline-20261020
BASE=/home/jetson/src/bpftime-array-jit-integrated-20261020/build-jit
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/regression-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/regression-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/regression-started-at.txt"
while ! test -f "$RESULTS/null-guard-exit-status.txt"; do sleep 3; done
test "$(<"$RESULTS/null-guard-exit-status.txt")" = 0
python3 "$RESULTS/archive-source.py"
for suite in array hash; do
    python3 -u "$RESULTS/run-regression.py" --baseline "$BASE" --candidate "$ROOT/build-jit" \
        --candidate-inline --suite "$suite" --output "$RESULTS/$suite-regression3" --runs 3 --iterations 50000
done
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import json,sys
r=Path(sys.argv[1])
for suite in ('array','hash'):
    for row in json.loads((r/f'{suite}-regression3/summary.json').read_text()):
        if row['historically_comparable']:
            assert row['candidate']['median']/row['baseline']['median']<=1.03,row
print('Non-target regression gates passed.')
PY
