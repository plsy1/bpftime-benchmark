#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/y1/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/y1/src/benchmark-results/uprobe/kernel-array-update-sizes-x64-20260803-fixedfreq}
CPU=5
SIBLING=11
ITERATIONS=20000
ROUNDS=5
WARMUP=1000
EXPECTED_COMMIT=176eb291ead95eb5f8a56280deae626fac46eaa9
LOADER="$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes-victim"
BPFTOOL=${BPFTOOL:-/home/y1/src/.toolchains/bpftool-profile-v2/bpftool}
PSTATE=/sys/devices/system/cpu/intel_pstate
TURBOSTAT_COLUMNS=CPU,Avg_MHz,Busy%,Bzy_MHz,TSC_MHz,CoreTmp,PkgTmp,PkgWatt

mkdir -p "$OUT/raw-wall"
sudo -n true
[[ $(git -C "$ROOT" rev-parse HEAD) == "$EXPECTED_COMMIT" ]]

sibling_before=$(cat "/sys/devices/system/cpu/cpu${SIBLING}/online")
no_turbo_before=$(cat "$PSTATE/no_turbo")
min_perf_before=$(cat "$PSTATE/min_perf_pct")
max_perf_before=$(cat "$PSTATE/max_perf_pct")
bpf_stats_before=$(sysctl -n kernel.bpf_stats_enabled)
declare -A governor_before
for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    governor_before["$policy"]=$(cat "$policy/scaling_governor")
done

cleanup() {
    set +e
    sudo -n sysctl -q -w "kernel.bpf_stats_enabled=$bpf_stats_before"
    if [[ "$sibling_before" == 1 ]]; then
        echo 1 | sudo -n tee "/sys/devices/system/cpu/cpu${SIBLING}/online" >/dev/null
    fi
    echo "$min_perf_before" | sudo -n tee "$PSTATE/min_perf_pct" >/dev/null
    echo "$max_perf_before" | sudo -n tee "$PSTATE/max_perf_pct" >/dev/null
    echo "$no_turbo_before" | sudo -n tee "$PSTATE/no_turbo" >/dev/null
    for policy in "${!governor_before[@]}"; do
        echo "${governor_before[$policy]}" | sudo -n tee "$policy/scaling_governor" >/dev/null
    done
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
}
trap cleanup EXIT INT TERM

for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    echo performance | sudo -n tee "$policy/scaling_governor" >/dev/null
done
echo 1 | sudo -n tee "$PSTATE/no_turbo" >/dev/null
echo 100 | sudo -n tee "$PSTATE/max_perf_pct" >/dev/null
echo 100 | sudo -n tee "$PSTATE/min_perf_pct" >/dev/null
echo 0 | sudo -n tee "/sys/devices/system/cpu/cpu${SIBLING}/online" >/dev/null
sudo -n sysctl -q -w kernel.bpf_stats_enabled=1

sudo -n turbostat --quiet --cpu "$CPU" \
    --out "$OUT/raw-wall/turbostat.txt" \
    --show "$TURBOSTAT_COLUMNS" -- \
    taskset -c "$CPU" "$LOADER" "$VICTIM" \
    "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
    >"$OUT/raw-wall/raw.csv" 2>"$OUT/raw-wall/stderr.txt"

[[ $(sudo -n "$BPFTOOL" link show | wc -l) -eq 0 ]]
