#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/jetson/src/bpftime-offical-no-btf
OUT=/home/jetson/src/benchmark-results/uprobe/kernel-map-runtime-arm64-20260803-profile
METRIC=${1:-cycles}
case "$METRIC" in
    cycles|instructions|l1d_loads|llc_misses) ;;
    *) echo "unsupported metric: $METRIC" >&2; exit 2 ;;
esac
RAW="$OUT/raw-profile-$METRIC"
KMR="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim"
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool
CPU=5
ITERATIONS=20000
ROUNDS=5
WARMUP=1000
PROFILE_SECONDS=30
EXPECTED_COMMIT=8ed291e130fe3f15f99955b0d259eb119efdaa7d

mkdir -p "$RAW"
sudo -n true
if [[ $(git -C "$ROOT" rev-parse HEAD) != "$EXPECTED_COMMIT" ]]; then
    echo "unexpected source commit" >&2
    exit 1
fi
if ! sudo -n nvpmodel -q 2>&1 | grep -q MAXN_SUPER; then
    echo "Jetson is not in MAXN_SUPER mode" >&2
    exit 1
fi

bpf_stats_before=$(sysctl -n kernel.bpf_stats_enabled)
loader_pid=
profile_pids=()
cleanup() {
    set +e
    for pid in "${profile_pids[@]}"; do
        sudo -n kill "$pid" 2>/dev/null
        wait "$pid" 2>/dev/null
    done
    if [[ -n "$loader_pid" ]]; then
        sudo -n kill "$loader_pid" 2>/dev/null
        wait "$loader_pid" 2>/dev/null
    fi
    sudo -n sysctl -q -w "kernel.bpf_stats_enabled=$bpf_stats_before"
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
}
trap cleanup EXIT INT TERM

sudo -n jetson_clocks
sudo -n sysctl -q -w kernel.bpf_stats_enabled=1

{
    echo "timestamp=$(date --iso-8601=ns)"
    echo "metric=$METRIC"
    echo "cpu=$CPU"
    echo "iterations=$ITERATIONS"
    echo "rounds=$ROUNDS"
    echo "warmup=$WARMUP"
    echo "profile_seconds=$PROFILE_SECONDS"
    echo "cpu_current_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_cur_freq)"
    echo "cpu_min_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_min_freq)"
    echo "cpu_max_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_max_freq)"
    echo "kernel.bpf_stats_enabled=$(sysctl -n kernel.bpf_stats_enabled)"
    sudo -n jetson_clocks --show
} >"$RAW/fixed-frequency-state.txt" 2>&1

before_max=$(sudo -n "$BPFTOOL" -j prog show | jq 'map(.id) | max // 0')

sudo -n taskset -c "$CPU" "$KMR" "$VICTIM" \
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
    if ! sudo -n kill -0 "$loader_pid" 2>/dev/null; then
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
    sudo -n "$BPFTOOL" prog dump xlated id "$id" \
        >"$RAW/prog-${id}-xlated.txt"
    sudo -n "$BPFTOOL" prog dump jited id "$id" \
        >"$RAW/prog-${id}-jited.txt" \
        2>"$RAW/prog-${id}-jited.stderr.txt" || true
done
if rg -q 'call' "$RAW/prog-${control_id}-xlated.txt" || \
    ! rg -q 'call.*map_update_elem|call 0x2|call 2' "$RAW/prog-${real_id}-xlated.txt"; then
    echo "array-update control/real disassembly validation failed" >&2
    exit 1
fi
printf 'control_id=%s\nreal_id=%s\n' "$control_id" "$real_id" \
    >"$RAW/program-ids.txt"

sudo -n "$BPFTOOL" -j prog profile id "$control_id" \
    duration "$PROFILE_SECONDS" "$METRIC" \
    >"$RAW/control-profile.json" 2>"$RAW/control-profile.stderr.txt" &
profile_pids+=("$!")
sudo -n "$BPFTOOL" -j prog profile id "$real_id" \
    duration "$PROFILE_SECONDS" "$METRIC" \
    >"$RAW/real-profile.json" 2>"$RAW/real-profile.stderr.txt" &
profile_pids+=("$!")

wait "$loader_pid"
loader_pid=
for pid in "${profile_pids[@]}"; do
    wait "$pid"
done
profile_pids=()

for json in "$RAW/control-profile.json" "$RAW/real-profile.json"; do
    if ! jq -e --arg metric "$METRIC" \
        'type == "array" and length == 1 and .[0].metric == $metric and
         .[0].run_cnt > 0 and .[0].enabled == .[0].running' \
        "$json" >/dev/null; then
        echo "invalid or unsupported bpftool profile output: $json" >&2
        cat "$json" >&2
        exit 1
    fi
done

if [[ $(sudo -n "$BPFTOOL" link show | wc -l) -ne 0 ]]; then
    echo "BPF links remained after profile run" >&2
    exit 1
fi

sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
