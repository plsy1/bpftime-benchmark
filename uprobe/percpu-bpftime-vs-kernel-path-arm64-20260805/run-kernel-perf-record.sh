#!/usr/bin/env bash
set -euo pipefail

SRC=/home/jetson/src/bpftime-offical-no-btf
OUT=/home/jetson/src/benchmark-results/uprobe/percpu-bpftime-vs-kernel-path-arm64-20260805
RAW="$OUT/raw-perf/kernel-cycles-record"
KPMR="$SRC/benchmark/uprobe/.output/kernel-percpu-map-runtime/kernel-percpu-map-runtime"
VICTIM="$SRC/benchmark/uprobe/.output/kernel-percpu-map-runtime/kernel-percpu-map-runtime-victim"
PERF=/usr/lib/linux-tools-6.8.0-134/perf
CPU=5
ITERATIONS=20000
ROUNDS=10
WARMUP=1000

test "$(git -C "$SRC" rev-parse HEAD)" = "1ee2eb6e2e2eacfc5a3ec7a3f8769adfdae6d492"
test -x "$KPMR" -a -x "$VICTIM" -a -x "$PERF"
sudo -n true
mkdir -p "$RAW"
OLD_STATS=$(sysctl -n kernel.bpf_stats_enabled)
cleanup() {
    set +e
    sudo -n sysctl -q -w "kernel.bpf_stats_enabled=$OLD_STATS"
}
trap cleanup EXIT INT TERM
sudo -n jetson_clocks
sudo -n sysctl -q -w kernel.bpf_stats_enabled=1
printf 'source_commit=%s\ncpu=%s iterations=%s rounds=%s warmup=%s\n' \
    "$(git -C "$SRC" rev-parse HEAD)" "$CPU" "$ITERATIONS" "$ROUNDS" "$WARMUP" \
    >"$RAW/command.txt"

sudo -n "$PERF" record -C "$CPU" -e cycles:k -g --call-graph fp \
    -o "$RAW/perf.data" -- \
    taskset -c "$CPU" "$KPMR" "$VICTIM" \
    "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
    >"$RAW/loader.csv" 2>"$RAW/loader.stderr.txt"

sudo -n "$PERF" report --stdio --no-children -i "$RAW/perf.data" \
    >"$RAW/perf-report.txt" 2>"$RAW/perf-report.stderr.txt" || true
sudo -n chown -R "$(id -u):$(id -g)" "$RAW"

test "$(grep -c '^array,array_lookup,control,' "$RAW/loader.csv")" -eq "$ROUNDS"
test "$(grep -c '^percpu_hash,hash_update,real-minus-control,' "$RAW/loader.csv")" -eq "$ROUNDS"
test -s "$RAW/perf.data"
