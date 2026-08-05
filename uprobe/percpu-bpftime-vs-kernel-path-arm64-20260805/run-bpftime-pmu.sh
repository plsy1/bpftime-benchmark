#!/usr/bin/env bash
set -euo pipefail

SRC=/home/jetson/src/bpftime-offical-no-btf
OUT=/home/jetson/src/benchmark-results/uprobe/percpu-bpftime-vs-kernel-path-arm64-20260805
BIN="$SRC/build-array-layers/benchmark/uprobe/diagnostics/per-cpu-helper-jit-layers"
PERF=/usr/lib/linux-tools-6.8.0-134/perf
CPU=5
INVOCATIONS=50000

test -x "$BIN" -a -x "$PERF"
sudo -n true
sudo -n jetson_clocks

for spec in \
    "array lookup" "array update" \
    "percpu_array lookup" "percpu_array update" \
    "hash lookup" "hash update" \
    "percpu_hash lookup" "percpu_hash update"; do
    read -r kind operation <<<"$spec"
    for mode in noop real; do
        sudo -n "$PERF" stat -x, \
            -e task-clock,cycles,instructions,branches,branch-misses \
            -r 3 -- taskset -c "$CPU" "$BIN" "$kind" "$operation" "$mode" \
            "$INVOCATIONS" 1 "$CPU" \
            >"$OUT/raw-bpftime/${kind}-${operation}-${mode}.stdout.txt" \
            2>"$OUT/raw-bpftime/${kind}-${operation}-${mode}.perf.txt"
    done
done
