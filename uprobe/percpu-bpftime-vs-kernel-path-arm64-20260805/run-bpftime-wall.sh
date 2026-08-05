#!/usr/bin/env bash
set -euo pipefail

SRC=/home/jetson/src/bpftime-offical-no-btf
OUT=/home/jetson/src/benchmark-results/uprobe/percpu-bpftime-vs-kernel-path-arm64-20260805
BIN="$SRC/build-array-layers/benchmark/uprobe/diagnostics/per-cpu-helper-jit-layers"
CPU=5
ROUNDS=5

test "$(git -C "$SRC" rev-parse HEAD)" = "e0240a1a81c461f758d3db9fcb8d159e2d9dcf98"
test -x "$BIN"
sudo -n true
sudo -n jetson_clocks

for spec in \
    "array lookup 100000" "array update 100000" \
    "percpu_array lookup 100000" "percpu_array update 100000" \
    "hash lookup 20000" "hash update 20000" \
    "percpu_hash lookup 20000" "percpu_hash update 20000"; do
    read -r kind operation invocations <<<"$spec"
    sudo -n taskset -c "$CPU" "$BIN" "$kind" "$operation" pair \
        "$invocations" "$ROUNDS" "$CPU" \
        >"$OUT/raw-bpftime/${kind}-${operation}-pair.txt" \
        2>"$OUT/raw-bpftime/${kind}-${operation}-pair.stderr.txt"
done
