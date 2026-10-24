#!/usr/bin/env bash
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/array-jit-default-20261021/hash-delete-investigation
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/ab-exit-status.txt"; date --iso-8601=seconds >"$OUT/ab-finished-at.txt"' EXIT
test "$(<"$OUT/build-exit-status.txt")" = 0
python3 -u "$OUT/run-ab.py" --rounds 12 --iterations 10000 --warmup 1000
