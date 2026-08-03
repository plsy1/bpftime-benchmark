#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/kernel-array-update-sizes-arm64-20260803}
BPFTOOL=${BPFTOOL:-/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool}
LOADER="$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes-victim"

{
    echo "timestamp=$(date --iso-8601=seconds)"
    echo "uname=$(uname -a)"
    echo "source_branch=$(git -C "$ROOT" branch --show-current)"
    echo "source_commit=$(git -C "$ROOT" rev-parse HEAD)"
    echo "source_status_begin"
    git -C "$ROOT" status --short
    echo "source_status_end"
    echo "cc=$(cc --version | head -n 1)"
    echo "clang=$(/usr/lib/llvm-15/bin/clang --version | head -n 1)"
    echo "bpftool=$($BPFTOOL version | tr '\n' ' ')"
    echo "loader_sha256=$(sha256sum "$LOADER" | awk '{print $1}')"
    echo "victim_sha256=$(sha256sum "$VICTIM" | awk '{print $1}')"
    echo "profile_bpftool_sha256=$(sha256sum "$BPFTOOL" | awk '{print $1}')"
    echo "cpu5_thread_siblings=$(cat /sys/devices/system/cpu/cpu5/topology/thread_siblings_list)"
    echo "cpu5_current_khz=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)"
    echo "cpu5_min_khz=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_min_freq)"
    echo "cpu5_max_khz=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_max_freq)"
    echo "kernel.bpf_stats_enabled=$(sysctl -n kernel.bpf_stats_enabled)"
    echo "bpf_link_count=$(sudo -n "$BPFTOOL" link show | wc -l)"
    sudo -n nvpmodel -q 2>&1
    sudo -n jetson_clocks --show 2>&1
    lscpu
} >"$OUT/environment.txt"
