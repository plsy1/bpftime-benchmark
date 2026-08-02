#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/jetson/src/bpftime-offical-no-btf
OUT=/home/jetson/src/benchmark-results/uprobe/kernel-map-runtime-arm64-20260803-profile
RAW="$OUT/raw-perf-record"
KMR="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim"
PERF=/usr/lib/linux-tools-6.8.0-134/perf
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool
CPU=5
ITERATIONS=20000
ROUNDS=5
WARMUP=1000

mkdir -p "$RAW"
sudo -n true
bpf_stats_before=$(sysctl -n kernel.bpf_stats_enabled)
cleanup() {
    set +e
    sudo -n sysctl -q -w "kernel.bpf_stats_enabled=$bpf_stats_before"
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
}
trap cleanup EXIT INT TERM

sudo -n jetson_clocks
sudo -n sysctl -q -w kernel.bpf_stats_enabled=1
sudo -n "$PERF" record -e cycles:k -g --call-graph fp \
    -o "$RAW/perf.data" -- \
    taskset -c "$CPU" "$KMR" "$VICTIM" \
    "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
    >"$RAW/raw.csv" 2>"$RAW/perf-record.stderr.txt"

sudo -n "$PERF" report --stdio --no-children -n \
    -i "$RAW/perf.data" >"$RAW/perf-report.txt" 2>"$RAW/perf-report.stderr.txt"
sudo -n "$BPFTOOL" link show >"$RAW/final-links.txt"
if [[ -s "$RAW/final-links.txt" ]]; then
    echo "BPF links remained after perf record" >&2
    exit 1
fi
sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
