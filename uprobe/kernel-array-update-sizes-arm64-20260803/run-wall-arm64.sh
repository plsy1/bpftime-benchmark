#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/kernel-array-update-sizes-arm64-20260803}
CPU=${CPU:-5}
ITERATIONS=${ITERATIONS:-20000}
ROUNDS=${ROUNDS:-5}
WARMUP=${WARMUP:-1000}
EXPECTED_COMMIT=176eb291ead95eb5f8a56280deae626fac46eaa9
LOADER="$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes-victim"
BPFTOOL=${BPFTOOL:-/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool}

mkdir -p "$OUT/raw-wall"
sudo -n true
[[ $(git -C "$ROOT" rev-parse HEAD) == "$EXPECTED_COMMIT" ]]
sudo -n nvpmodel -q 2>&1 | grep -q MAXN_SUPER
make -C "$ROOT/benchmark/uprobe" kernel-array-update-sizes-diagnostic \
    CLANG=/usr/lib/llvm-15/bin/clang

bpf_stats_before=$(sysctl -n kernel.bpf_stats_enabled)
cleanup() {
    sudo -n sysctl -q -w "kernel.bpf_stats_enabled=$bpf_stats_before"
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
}
trap cleanup EXIT INT TERM

sudo -n jetson_clocks
sudo -n sysctl -q -w kernel.bpf_stats_enabled=1
sudo -n taskset -c "$CPU" "$LOADER" "$VICTIM" \
    "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
    >"$OUT/raw-wall/raw.csv" 2>"$OUT/raw-wall/stderr.txt"

if [[ $(sudo -n "$BPFTOOL" link show | wc -l) -ne 0 ]]; then
    echo "BPF links remained after wall-time run" >&2
    exit 1
fi
