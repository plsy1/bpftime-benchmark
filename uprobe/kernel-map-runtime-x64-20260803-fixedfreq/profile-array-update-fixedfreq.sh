#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/y1/src/bpftime-offical-no-btf
OUT=/home/y1/src/benchmark-results/uprobe/kernel-map-runtime-x64-20260803-fixedfreq
METRIC=${1:-cycles}
case "$METRIC" in
    cycles|instructions|l1d_loads|llc_misses) ;;
    *) echo "unsupported metric: $METRIC" >&2; exit 2 ;;
esac
RAW="$OUT/raw-profile-$METRIC"
KMR="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim"
BPFTOOL=/home/y1/src/.toolchains/bpftool-profile-v2/bpftool
CPU=5
SIBLING=11
ITERATIONS=20000
ROUNDS=5
WARMUP=1000
PROFILE_SECONDS=30
PSTATE=/sys/devices/system/cpu/intel_pstate
TURBOSTAT_COLUMNS=CPU,Avg_MHz,Busy%,Bzy_MHz,TSC_MHz,CoreTmp,PkgTmp,PkgWatt

mkdir -p "$RAW"
sudo -n true

sibling_before=$(cat "/sys/devices/system/cpu/cpu${SIBLING}/online")
no_turbo_before=$(cat "$PSTATE/no_turbo")
min_perf_before=$(cat "$PSTATE/min_perf_pct")
max_perf_before=$(cat "$PSTATE/max_perf_pct")
bpf_stats_before=$(sysctl -n kernel.bpf_stats_enabled)
declare -A governor_before
for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    governor_before["$policy"]=$(cat "$policy/scaling_governor")
done
loader_pid=
profile_pids=()

cleanup() {
    set +e
    for pid in "${profile_pids[@]}"; do
        kill "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
    done
    if [[ -n "$loader_pid" ]]; then
        kill "$loader_pid" 2>/dev/null
        wait "$loader_pid" 2>/dev/null
    fi
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

before_max=$(sudo -n "$BPFTOOL" -j prog show | jq 'map(.id) | max // 0')

sudo -n turbostat --quiet --cpu "$CPU" \
    --out "$RAW/turbostat.txt" \
    --show "$TURBOSTAT_COLUMNS" -- \
    taskset -c "$CPU" "$KMR" "$VICTIM" \
    "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
    >"$RAW/raw.csv" 2>"$RAW/loader.stderr.txt" &
loader_pid=$!

control_id=
real_id=
for _ in $(seq 1 500); do
    programs=$(sudo -n "$BPFTOOL" -j prog show)
    control_id=$(jq -r --argjson baseline "$before_max" \
        '.[] | select(.id > $baseline and .name == "kmr_array_update_control") | .id' \
        <<<"$programs")
    real_id=$(jq -r --argjson baseline "$before_max" \
        '.[] | select(.id > $baseline and .name == "kmr_array_update_real") | .id' \
        <<<"$programs")
    if [[ -n "$control_id" && -n "$real_id" ]]; then
        break
    fi
    if ! kill -0 "$loader_pid" 2>/dev/null; then
        echo "loader exited before array-update programs were found" >&2
        exit 1
    fi
    sleep 0.01
done
if [[ -z "$control_id" || -z "$real_id" ]]; then
    echo "array-update control/real programs were not found" >&2
    exit 1
fi

for id in "$control_id" "$real_id"; do
    sudo -n "$BPFTOOL" prog dump xlated id "$id" >"$RAW/prog-${id}-xlated.txt"
    sudo -n "$BPFTOOL" prog dump jited id "$id" \
        >"$RAW/prog-${id}-jited.txt" \
        2>"$RAW/prog-${id}-jited.stderr.txt" || true
done
if rg -q 'call' "$RAW/prog-${control_id}-xlated.txt" || \
    ! rg -q 'call.*map_update_elem|call 0x2|call 2' "$RAW/prog-${real_id}-xlated.txt"; then
    echo "array-update control/real disassembly validation failed" >&2
    exit 1
fi
printf 'control_id=%s\nreal_id=%s\n' "$control_id" "$real_id" >"$RAW/program-ids.txt"

sudo -n "$BPFTOOL" -j prog profile id "$control_id" duration "$PROFILE_SECONDS" \
    "$METRIC" \
    >"$RAW/control-profile.json" 2>"$RAW/control-profile.stderr.txt" &
profile_pids+=("$!")
sudo -n "$BPFTOOL" -j prog profile id "$real_id" duration "$PROFILE_SECONDS" \
    "$METRIC" \
    >"$RAW/real-profile.json" 2>"$RAW/real-profile.stderr.txt" &
profile_pids+=("$!")

wait "$loader_pid"
loader_pid=
for pid in "${profile_pids[@]}"; do
    wait "$pid"
done
profile_pids=()
