#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020
ROOT=/home/jetson/src/bpftime-array-jit-integrated-20261020
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/final-check-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/final-check-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/final-check-started-at.txt"
python3 - "$RESULTS" "$ROOT" <<'PY'
from pathlib import Path
import hashlib,json,shutil,sys,time
r=Path(sys.argv[1]);source=Path(sys.argv[2]);deadline=time.monotonic()+1800
while True:
    status=r/'validation-final-exit-status.txt';finished=r/'validation-final-finished-at.txt';started=r/'validation-final-started-at.txt'
    if status.exists() and finished.exists() and finished.stat().st_mtime>=started.stat().st_mtime:
        assert status.read_text().strip()=='0','Second-version validation failed'
        break
    assert time.monotonic()<deadline
    time.sleep(3)
archive=r/'artifacts-before-reload-guard';archive.mkdir()
relocated=[]
for path,digest in json.loads((r/'ab6-final/environment.json').read_text())['binary_sha256'].items():
    p=Path(path)
    if source/'build-jit' not in p.parents:continue
    assert hashlib.sha256(p.read_bytes()).hexdigest()==digest
    dest=archive/p.name;shutil.copy2(p,dest)
    relocated.append(dict(original_path=path,archived_path=str(dest),sha256=digest))
(r/'artifacts-before-reload-guard.json').write_text(json.dumps(relocated,indent=2)+'\n')
shutil.copytree(r/'source',r/'source-before-reload-guard')
for name in ('RESULT.md','integration-vs-cpp-candidate.diff','source-sha256.json','tests.log','host-contract-tests.log'):
    shutil.copy2(r/name,r/('before-reload-guard-'+name))
print('Miss-fastpath evidence frozen; validating the final reload guard.',flush=True)
PY
bash "$RESULTS/run-build.sh"
bash "$RESULTS/run-validation-release.sh"
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import json,sys
r=Path(sys.argv[1])
assert (r/'validation-release-exit-status.txt').read_text().strip()=='0'
(r/'full-inputs-reviewed').write_text(json.dumps(dict(
    validation='validation-release', matched='ab6-release',
    source=json.loads((r/'source-sha256.json').read_text()),
    binary=json.loads((r/'ab6-release/environment.json').read_text())['binary_sha256']),indent=2)+'\n')
PY
bash "$RESULTS/run-full.sh"
