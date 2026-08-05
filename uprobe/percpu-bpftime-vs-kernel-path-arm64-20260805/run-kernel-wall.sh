#!/usr/bin/env bash
set -euo pipefail

SRC=/home/jetson/src/bpftime-offical-no-btf
OUT=/home/jetson/src/benchmark-results/uprobe/percpu-bpftime-vs-kernel-path-arm64-20260805
KPMR="$SRC/benchmark/uprobe/.output/kernel-percpu-map-runtime/kernel-percpu-map-runtime"
VICTIM="$SRC/benchmark/uprobe/.output/kernel-percpu-map-runtime/kernel-percpu-map-runtime-victim"
CPU=5
ITERATIONS=20000
ROUNDS=5
WARMUP=1000
RAW="$OUT/raw-kernel/kernel-runtime.csv"

test "$(git -C "$SRC" rev-parse HEAD)" = "e0240a1a81c461f758d3db9fcb8d159e2d9dcf98"
test -x "$KPMR" -a -x "$VICTIM"
sudo -n true
OLD_STATS=$(sysctl -n kernel.bpf_stats_enabled)
cleanup() {
    set +e
    sudo -n sysctl -q -w "kernel.bpf_stats_enabled=$OLD_STATS"
    sudo -n /home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool link show \
        >"$OUT/raw-kernel/final-links.txt" 2>&1
}
trap cleanup EXIT INT TERM

sudo -n jetson_clocks
sudo -n sysctl -q -w kernel.bpf_stats_enabled=1
{
    echo "timestamp=$(date --iso-8601=ns)"
    echo "source_commit=$(git -C "$SRC" rev-parse HEAD)"
    echo "cpu=$CPU iterations=$ITERATIONS rounds=$ROUNDS warmup=$WARMUP"
    echo "cpu_current_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_cur_freq)"
    echo "bpf_stats=$(sysctl -n kernel.bpf_stats_enabled)"
} >"$OUT/raw-kernel/state.txt"

sudo -n taskset -c "$CPU" "$KPMR" "$VICTIM" \
    "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
    >"$RAW" 2>"$OUT/raw-kernel/loader.stderr.txt"

if [[ -s "$OUT/raw-kernel/final-links.txt" ]]; then
    echo "kernel links remained" >&2
    exit 1
fi
