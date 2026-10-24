#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import sys,time
r=Path(sys.argv[1]);deadline=time.monotonic()+3600
while True:
    status=r/'validation-exit-status.txt';finished=r/'validation-finished-at.txt';started=r/'validation-started-at.txt'
    if status.exists() and finished.exists() and finished.stat().st_mtime>=started.stat().st_mtime:
        assert status.read_text().strip()=='0','Short validation failed; full run held for review'
        break
    assert time.monotonic()<deadline,'Timed out waiting for short validation'
    time.sleep(5)
PY
bash "$RESULTS/run-full.sh"
