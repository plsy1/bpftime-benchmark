#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-array-jit-poc-20261019
RESULTS=/home/jetson/src/results/uprobe/array-jit-inline-poc-20261019
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/short-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/short-finished-at.txt"' EXIT
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import sys,time
r=Path(sys.argv[1]); deadline=time.monotonic()+1200
while True:
    status=r/'build-exit-status.txt'; finished=r/'build-finished-at.txt'; started=r/'build-started-at.txt'
    if status.exists() and finished.exists() and finished.stat().st_mtime>=started.stat().st_mtime:
        assert status.read_text().strip()=='0','Independent build/tests failed; inspect build logs'
        break
    assert time.monotonic()<deadline,'Timed out waiting for build/tests'
    time.sleep(3)
PY
# The successful build already produced/tested all targets. Verify that the
# late diagnostic edit reached the agent instead of forcing another libbpf
# install/recompile of the entire dependency graph.
python3 - "$ROOT" <<'PY'
from pathlib import Path
import mmap,sys
r=Path(sys.argv[1])
source=r/'runtime/src/bpftime_prog.cpp'
obj=r/'build-poc/runtime/CMakeFiles/runtime.dir/src/bpftime_prog.cpp.o'
agent=r/'build-poc/runtime/agent/libbpftime-agent.so'
assert obj.stat().st_mtime>=source.stat().st_mtime
assert agent.stat().st_mtime>=obj.stat().st_mtime
with agent.open('rb') as f, mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as data:
    assert data.find(b'readable_window_bytes=')>=0
PY
"$ROOT/build-poc/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    >"$RESULTS/finalize-tests.log" 2>&1
python3 "$RESULTS/archive-source.py"
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/smoke-all" \
    --runs 1 --iterations 2000 --dump-jit
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/ab6" \
    --runs 6 --iterations 50000
printf 'Matched six-round four-engine POC test completed.\n'
