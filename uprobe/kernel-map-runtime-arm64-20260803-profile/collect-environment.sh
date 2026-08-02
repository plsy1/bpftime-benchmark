#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/jetson/src/bpftime-offical-no-btf
OUT=/home/jetson/src/benchmark-results/uprobe/kernel-map-runtime-arm64-20260803-profile
KMR="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim"
BPF_OBJECT="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel_map_runtime.bpf.o"
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool
PERF=/usr/lib/linux-tools-6.8.0-134/perf

{
    echo "Experiment date: $(date --iso-8601=seconds)"
    echo "Host: $(hostname)"
    echo "Kernel: $(uname -a)"
    echo "Architecture: $(uname -m)"
    echo
    echo "Source repository: $ROOT"
    echo "Source branch: $(git -C "$ROOT" branch --show-current)"
    echo "Source commit: $(git -C "$ROOT" rev-parse HEAD)"
    echo "Source status after experiment:"
    git -C "$ROOT" status --short
    echo
    /usr/bin/gcc-13 --version | head -1
    /usr/lib/llvm-15/bin/clang --version | head -1
    "$BPFTOOL" version
    "$PERF" --version
    echo "bpftool_sha256=$(sha256sum "$BPFTOOL" | awk '{print $1}')"
    echo "loader_sha256=$(sha256sum "$KMR" | awk '{print $1}')"
    echo "victim_sha256=$(sha256sum "$VICTIM" | awk '{print $1}')"
    echo "bpf_object_sha256=$(sha256sum "$BPF_OBJECT" | awk '{print $1}')"
    echo
    echo "CPU5 siblings: $(cat /sys/devices/system/cpu/cpu5/topology/thread_siblings_list)"
    echo "CPU5 governor: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_governor)"
    echo "CPU5 min kHz: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_min_freq)"
    echo "CPU5 max kHz: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_max_freq)"
    echo "CPU5 current kHz: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)"
    sudo -n nvpmodel -q
    sudo -n jetson_clocks --show
    echo
    echo "kernel.bpf_stats_enabled after experiment: $(sysctl -n kernel.bpf_stats_enabled)"
    echo "kernel BPF links after experiment: $(sudo -n "$BPFTOOL" link show | wc -l)"
    echo "kernel BTF exists: $([[ -e /sys/kernel/btf/vmlinux ]] && echo yes || echo no)"
    echo "/proc/kcore exists: $([[ -e /proc/kcore ]] && echo yes || echo no)"
    echo "function tracer available: $(sudo -n test -e /sys/kernel/tracing/available_filter_functions && echo yes || echo no)"
    echo
    echo "Profile parameters: 20000 invocations, 5 rounds, 1000 warm-up"
    echo "Profile duration: 30 seconds per metric"
    echo "Profile metrics: cycles, instructions, l1d_loads, llc_misses"
    echo "Target: array-update control/real BPF programs, 1000 loop iterations per invocation"
    echo "CPU: loader, victim, and target BPF execution pinned to CPU5"
    echo "Privilege: root"
    echo "Power: MAXN_SUPER and jetson_clocks"
    echo "Kernel disassembly: /boot/Image mapped with root /proc/kallsyms"
} >"$OUT/environment.txt" 2>&1
