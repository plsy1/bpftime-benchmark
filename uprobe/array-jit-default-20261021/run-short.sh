#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/array-jit-default-20261021
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/short-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/short-finished-at.txt"' EXIT
test "$(<"$RESULTS/build-exit-status.txt")" = 0
# Compile standalone BEFORE any timing; all objects use the generated header.
/usr/bin/g++-13 -std=c++20 -O2 \
    -I"$ROOT/build-jit/vm/compat/llvm-vm/array-inline-include" \
    -I"$ROOT/vm/llvm-jit/include" -I"$ROOT/vm/llvm-jit/src" -I"$ROOT/vm/compat/include" \
    "$RESULTS/standalone-vm.cpp" "$ROOT/build-jit/vm/compat/llvm-vm/libllvmbpf_vm.a" \
    "$ROOT/build-jit/third_party/spdlog/libspdlog.a" -lLLVM-16 -lpthread -ldl -lz -ltinfo \
    -o "$RESULTS/standalone-vm" >"$RESULTS/build-standalone.log" 2>&1
"$RESULTS/standalone-vm" >"$RESULTS/standalone.log" 2>&1
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/percpu-ab6" --runs 6 --iterations 50000 --dump-jit
python3 -u "$RESULTS/run-ordinary.py" --output "$RESULTS/ordinary-ab6" --runs 6 --iterations 50000 --dump-jit
python3 - "$RESULTS" <<'PY'
import json,sys
from pathlib import Path
root=Path(sys.argv[1])
for name in ('percpu-ab6','ordinary-ab6'):
    for row in json.loads((root/name/'summary.json').read_text()):
        if row['case']!='unused':
            assert row['inline_vs_previous_percent']<=3,(name,row)
print('Default-on consuming checks and <=3% non-regression gate passed.')
PY
