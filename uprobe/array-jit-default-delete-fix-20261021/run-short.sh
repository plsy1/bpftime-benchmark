#!/usr/bin/env bash
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/array-jit-default-delete-fix-20261021
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/short-exit-status.txt"; date --iso-8601=seconds >"$OUT/short-finished-at.txt"' EXIT
test "$(cat "$OUT/build-exit-status.txt")" = 0
date --iso-8601=seconds >"$OUT/short-started-at.txt"
python3 -u "$OUT/run-screen.py" --rounds 12 --iterations 10000 >"$OUT/screen.log" 2>&1
python3 -u "$OUT/run-consuming.py" ordinary --output "$OUT/ordinary-consuming" --runs 6 --iterations 50000 >"$OUT/ordinary-consuming.log" 2>&1
python3 -u "$OUT/run-consuming.py" percpu --output "$OUT/percpu-consuming" --runs 6 --iterations 50000 >"$OUT/percpu-consuming.log" 2>&1
