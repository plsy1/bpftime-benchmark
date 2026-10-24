#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020
ROOT=/home/jetson/src/bpftime-array-jit-integrated-20261020
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/release-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/release-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/release-started-at.txt"
python3 - "$RESULTS" "$ROOT" <<'PY'
from pathlib import Path
import hashlib,json,shutil,sys,time
r=Path(sys.argv[1]); source=Path(sys.argv[2]); deadline=time.monotonic()+1200
while True:
    status=r/'validation-exit-status.txt'; finished=r/'validation-finished-at.txt'; started=r/'validation-started-at.txt'
    if status.exists() and finished.exists() and finished.stat().st_mtime>=started.stat().st_mtime:
        assert status.read_text().strip()=='0','First-version regression did not complete successfully'
        break
    assert time.monotonic()<deadline
    time.sleep(3)
archive=r/'artifacts-before-miss-fastpath';archive.mkdir()
relocated=[]
for path,digest in json.loads((r/'ab6/environment.json').read_text())['binary_sha256'].items():
    original=Path(path)
    if source/'build-jit' not in original.parents:continue
    assert hashlib.sha256(original.read_bytes()).hexdigest()==digest
    target=archive/original.name;shutil.copy2(original,target)
    relocated.append(dict(original_path=path,archived_path=str(target),sha256=digest))
(r/'artifacts-before-miss-fastpath.json').write_text(json.dumps(relocated,indent=2)+'\n')
shutil.copytree(r/'source',r/'source-before-miss-fastpath')
shutil.copy2(r/'tests.log',r/'tests-before-miss-fastpath.log')
shutil.copy2(r/'host-contract-tests.log',r/'host-contract-tests-before-miss-fastpath.log')
print('First-version artifacts frozen; rebuilding the final miss fast path.',flush=True)
PY
bash "$RESULTS/run-build.sh"
bash "$RESULTS/run-validation-final.sh"
bash "$RESULTS/run-full.sh"
