#!/usr/bin/env bash
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/array-jit-default-20261021/hash-delete-recheck
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/exit-status.txt"; date --iso-8601=seconds >"$OUT/finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"
gcc --version >"$OUT/compiler.txt"
# Separate objects, no LTO: preserve the original exported probe stubs and
# original setup/timer implementation; only the entry point selects one case.
gcc "$ROOT/benchmark/test.c" -g -O3 -Dmain=full_suite_main -c -o "$OUT/original-victim.o" >"$OUT/compile.log" 2>&1
gcc "$OUT/single-main.c" -g -O3 -c -o "$OUT/single-main.o" >>"$OUT/compile.log" 2>&1
gcc "$OUT/original-victim.o" "$OUT/single-main.o" -lpthread -o "$OUT/runner/benchmark/test" >>"$OUT/compile.log" 2>&1
python3 -u "$OUT/run.py" --rounds 12 --iterations 10000 --warmup 1000
