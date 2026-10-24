#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020
ROOT=/home/jetson/src/bpftime-array-jit-integrated-20261020
FROZEN=/tmp/bpftime-array-offset-branch-20261019.z1JKDF/branch
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/accepted-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/accepted-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/accepted-started-at.txt"
python3 - "$RESULTS" "$ROOT" <<'PY'
from pathlib import Path
import hashlib,json,shutil,sys,time
r=Path(sys.argv[1]);source=Path(sys.argv[2]);deadline=time.monotonic()+1800
while True:
    status=r/'validation-release-exit-status.txt';finished=r/'validation-release-finished-at.txt';started=r/'validation-release-started-at.txt'
    if status.exists() and finished.exists() and finished.stat().st_mtime>=started.stat().st_mtime:
        assert status.read_text().strip()=='0','Reload-guard validation failed'
        break
    assert time.monotonic()<deadline
    time.sleep(3)
archive=r/'artifacts-before-fork-guard';archive.mkdir()
relocated=[]
for path,digest in json.loads((r/'ab6-release/environment.json').read_text())['binary_sha256'].items():
    p=Path(path)
    if source/'build-jit' not in p.parents:continue
    assert hashlib.sha256(p.read_bytes()).hexdigest()==digest
    dest=archive/p.name;shutil.copy2(p,dest)
    relocated.append(dict(original_path=path,archived_path=str(dest),sha256=digest))
(r/'artifacts-before-fork-guard.json').write_text(json.dumps(relocated,indent=2)+'\n')
shutil.copytree(r/'source',r/'source-before-fork-guard')
for name in ('RESULT.md','integration-vs-cpp-candidate.diff','source-sha256.json','tests.log','host-contract-tests.log'):
    shutil.copy2(r/name,r/('before-fork-guard-'+name))
print('Earlier evidence frozen; building the final fork-safe source.',flush=True)
PY
bash "$RESULTS/run-build.sh"
timeout 180 "$ROOT/build-jit/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' \
    >"$RESULTS/host-contract-tests.log" 2>&1
python3 "$RESULTS/archive-source.py"
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/smoke-accepted" --runs 1 --iterations 2000 --dump-jit
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/ab6-accepted" --runs 6 --iterations 50000
python3 "$RESULTS/analyze-accepted.py" >"$RESULTS/analysis-accepted.log"
for suite in array hash; do
    python3 -u "$RESULTS/run-regression.py" --baseline "$FROZEN" --candidate "$ROOT/build-jit" \
        --candidate-inline --suite "$suite" --output "$RESULTS/$suite-regression3" --runs 3 --iterations 50000
done
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import json,sys
r=Path(sys.argv[1])
(r/'validation-accepted-exit-status.txt').write_text('0\n')
(r/'full-inputs-reviewed').write_text(json.dumps(dict(
    validation='validation-accepted',matched='ab6-accepted',
    source=json.loads((r/'source-sha256.json').read_text()),
    binary=json.loads((r/'ab6-accepted/environment.json').read_text())['binary_sha256']),indent=2)+'\n')
PY
bash "$RESULTS/run-full.sh"
