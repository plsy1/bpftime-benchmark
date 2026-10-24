#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-array-jit-integrated-20261020
RESULTS=/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020
FROZEN=/tmp/bpftime-array-offset-branch-20261019.z1JKDF/branch
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/validation-release-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/validation-release-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/validation-release-started-at.txt"
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import sys,time
r=Path(sys.argv[1]); deadline=time.monotonic()+1800
while True:
    status=r/'build-exit-status.txt'; finished=r/'build-finished-at.txt'; started=r/'build-started-at.txt'
    if status.exists() and finished.exists() and finished.stat().st_mtime>=started.stat().st_mtime:
        assert status.read_text().strip()=='0','Build/tests failed'
        break
    assert time.monotonic()<deadline,'Initial build timed out'
    time.sleep(3)
PY
timeout 180 "$ROOT/build-jit/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' \
    >"$RESULTS/host-contract-tests.log" 2>&1
python3 "$RESULTS/archive-source.py"
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/smoke-release" \
    --runs 1 --iterations 2000 --dump-jit
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/ab6-release" \
    --runs 6 --iterations 50000
python3 "$RESULTS/analyze-release.py" >"$RESULTS/analysis-release.log"
for suite in array hash; do
    python3 -u "$RESULTS/run-regression.py" --baseline "$FROZEN" \
        --candidate "$ROOT/build-jit" --candidate-inline --suite "$suite" \
        --output "$RESULTS/$suite-regression6-release" --runs 6 --iterations 50000
done
printf 'Lifecycle, consuming JIT benchmark and non-target regression checks completed.\n'
