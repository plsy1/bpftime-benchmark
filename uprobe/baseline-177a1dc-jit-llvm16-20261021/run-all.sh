#!/usr/bin/env bash
# Serial pipeline: build/tests -> ordinary/per-CPU consuming -> full ten rounds.
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/baseline-177a1dc-jit-llvm16-20261021
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/exit-status.txt"; date --iso-8601=seconds >"$OUT/finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"
bash "$OUT/run-build.sh"
python3 -u "$OUT/run-consuming.py" ordinary --output "$OUT/ordinary-consuming" --runs 6 --iterations 50000 \
    >"$OUT/ordinary-consuming.log" 2>&1
python3 -u "$OUT/run-consuming.py" percpu --output "$OUT/percpu-consuming" --runs 6 --iterations 50000 \
    >"$OUT/percpu-consuming.log" 2>&1
bash "$OUT/run-full.sh" >"$OUT/full-console.log" 2>&1
