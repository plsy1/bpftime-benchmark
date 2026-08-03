#!/usr/bin/env bash
set -euo pipefail

OPERATION=${1:?usage: profile-kernel-lookup-pmu.sh <array_lookup|hash_lookup> <cycles|instructions>}
METRIC=${2:?usage: profile-kernel-lookup-pmu.sh <array_lookup|hash_lookup> <cycles|instructions>}
case "$OPERATION" in
    array_lookup|hash_lookup) ;;
    *) echo "unsupported operation: $OPERATION" >&2; exit 2 ;;
esac
case "$METRIC" in
    cycles|instructions) ;;
    *) echo "unsupported metric: $METRIC" >&2; exit 2 ;;
esac

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/arm64-bpftime-vs-kernel-paths-20260803}
RAW="$OUT/raw-kernel-pmu/${OPERATION}-${METRIC}"
KMR="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-map-runtime/kernel-map-runtime-victim"
BPFTOOL=${BPFTOOL:-/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool}
CPU=5
ITERATIONS=20000
ROUNDS=5
WARMUP=1000
PROFILE_SECONDS=30
EXPECTED_COMMIT=176eb291ead95eb5f8a56280deae626fac46eaa9

mkdir -p "$RAW"
sudo -n true
[[ $(git -C "$ROOT" rev-parse HEAD) == "$EXPECTED_COMMIT" ]]
sudo -n nvpmodel -q 2>&1 | grep -q MAXN_SUPER
[[ -x "$BPFTOOL" && -x "$KMR" && -x "$VICTIM" ]]

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
    echo "operation=$OPERATION"
    echo "metric=$METRIC"
    echo "cpu=$CPU"
    echo "iterations=$ITERATIONS"
    echo "rounds=$ROUNDS"
    echo "warmup=$WARMUP"
    echo "cpu_current_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_cur_freq)"
    echo "cpu_min_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_min_freq)"
    echo "cpu_max_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_max_freq)"
    sudo -n jetson_clocks --show
} >"$RAW/frequency.txt" 2>&1

before_max=$(sudo -n "$BPFTOOL" -j prog show | jq 'map(.id) | max // 0')
sudo -n taskset -c "$CPU" "$KMR" "$VICTIM" \
    "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
    >"$RAW/raw.csv" 2>"$RAW/loader.stderr.txt" &
loader_pid=$!

control_name="kmr_${OPERATION}_control"
real_name="kmr_${OPERATION}_real"
control_id=
real_id=
for _ in $(seq 1 500); do
    programs=$(sudo -n "$BPFTOOL" -j prog show)
    control_id=$(jq -r --argjson base "$before_max" --arg name "$control_name" \
        '.[] | select(.id > $base and .name == $name) | .id' <<<"$programs")
    real_id=$(jq -r --argjson base "$before_max" --arg name "$real_name" \
        '.[] | select(.id > $base and .name == $name) | .id' <<<"$programs")
    [[ -n "$control_id" && -n "$real_id" ]] && break
    sudo -n kill -0 "$loader_pid" 2>/dev/null || {
        echo "loader exited before target programs were found" >&2
        exit 1
    }
    sleep 0.01
done
[[ -n "$control_id" && -n "$real_id" ]]
printf 'implementation,program_id\ncontrol,%s\nreal,%s\n' \
    "$control_id" "$real_id" >"$RAW/program-ids.csv"

for pair in "control:$control_id" "real:$real_id"; do
    impl=${pair%%:*}
    id=${pair##*:}
    sudo -n "$BPFTOOL" prog dump xlated id "$id" >"$RAW/${impl}-xlated.txt"
    sudo -n "$BPFTOOL" prog dump jited id "$id" \
        >"$RAW/${impl}-jited.txt" 2>"$RAW/${impl}-jited.stderr.txt" || true
    sudo -n "$BPFTOOL" -j prog profile id "$id" \
        duration "$PROFILE_SECONDS" "$METRIC" \
        >"$RAW/${impl}.json" 2>"$RAW/${impl}.stderr.txt" &
    profile_pids+=("$!")
done

wait "$loader_pid"
loader_pid=
for pid in "${profile_pids[@]}"; do
    wait "$pid"
done
profile_pids=()

control_runs=$(jq -r '.[0].run_cnt' "$RAW/control.json")
real_runs=$(jq -r '.[0].run_cnt' "$RAW/real.json")
[[ "$control_runs" -gt 0 && "$control_runs" -eq "$real_runs" ]]
for json in "$RAW/control.json" "$RAW/real.json"; do
    jq -e --arg metric "$METRIC" \
        'type == "array" and length == 1 and .[0].metric == $metric and
         .[0].enabled == .[0].running' "$json" >/dev/null
done

if [[ $(sudo -n "$BPFTOOL" link show | wc -l) -ne 0 ]]; then
    echo "BPF links remained after profile run" >&2
    exit 1
fi
