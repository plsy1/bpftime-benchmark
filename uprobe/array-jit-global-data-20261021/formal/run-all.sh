#!/usr/bin/env bash
# Serial pipeline: build/tests -> four consuming runs -> full ten rounds.
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/array-jit-global-data-20261021/formal
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/exit-status.txt"; date --iso-8601=seconds >"$OUT/finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"
bash "$OUT/run-build.sh"
for suite in ordinary-standard percpu-standard ordinary-globals percpu-globals; do
    python3 -u "$OUT/run-consuming-$suite.py" --output "$OUT/$suite" --runs 6 --iterations 50000 \
        >"$OUT/$suite.log" 2>&1
done
bash "$OUT/run-full.sh" >"$OUT/full-console.log" 2>&1
