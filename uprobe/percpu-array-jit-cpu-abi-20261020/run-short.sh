#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/percpu-array-jit-cpu-abi-20261020
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/short-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/short-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/short-started-at.txt"
while ! test -f "$RESULTS/cpu-abi-exit-status.txt"; do sleep 3; done
test "$(<"$RESULTS/cpu-abi-exit-status.txt")" = 0
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/smoke" --runs 1 --iterations 2000 --dump-jit
python3 "$RESULTS/check-jit.py"
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/ab6" --runs 6 --iterations 50000
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import json,sys
r=Path(sys.argv[1])
rows=json.loads((r/'ab6/summary.json').read_text())
hit=next(x for x in rows if x['case']=='hit')
assert hit['inline_vs_previous_percent']<=-2,hit
assert hit['paired_wins_vs_previous']>=5,hit
for row in rows:
    if row['case']!='unused': assert row['inline_vs_previous_percent']<=3,row
print('Consuming target gate passed.')
PY
python3 -u "$RESULTS/run-ordinary.py" --output "$RESULTS/ordinary-ab3" --runs 3 --iterations 50000
printf 'CPU ABI consuming short tests completed.\n'
