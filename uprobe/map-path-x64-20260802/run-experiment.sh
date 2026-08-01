#!/usr/bin/env bash
set -euo pipefail

CPU=${CPU:-5}
SIBLING=${SIBLING:-11}
RESULT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CODE_DIR=/home/y1/src/bpftime-offical-no-btf
BIN_DIR="$CODE_DIR/build-map-path-x64-20260802/benchmark/uprobe/diagnostics"
EVENTS=task-clock,cycles,instructions,branches,branch-misses,cache-references,cache-misses

mkdir -p "$RESULT_DIR/raw-perf" "$RESULT_DIR/raw-stdout"

turbo_file=/sys/devices/system/cpu/intel_pstate/no_turbo
turbo_before=$(cat "$turbo_file")
sibling_before=$(cat "/sys/devices/system/cpu/cpu$SIBLING/online")
declare -A governors_before=()
for governor in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
    governors_before["$governor"]=$(cat "$governor")
done

restore_system_state() {
    for governor in "${!governors_before[@]}"; do
        printf '%s' "${governors_before[$governor]}" | sudo tee "$governor" >/dev/null || true
    done
    printf '%s' "$turbo_before" | sudo tee "$turbo_file" >/dev/null || true
    printf '%s' "$sibling_before" | sudo tee "/sys/devices/system/cpu/cpu$SIBLING/online" >/dev/null || true
}
trap restore_system_state EXIT INT TERM

for governor in "${!governors_before[@]}"; do
    printf '%s' performance | sudo tee "$governor" >/dev/null
done
printf '1' | sudo tee "$turbo_file" >/dev/null
printf '0' | sudo tee "/sys/devices/system/cpu/cpu$SIBLING/online" >/dev/null

{
    echo "benchmark_cpu=$CPU"
    echo "offline_smt_sibling=$SIBLING"
    echo "sibling_online_before=$sibling_before"
    echo "turbo_no_turbo_before=$turbo_before"
    echo "turbo_no_turbo_during=$(cat "$turbo_file")"
    for governor in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
        echo "$governor=$(cat "$governor")"
    done
} > "$RESULT_DIR/system-state-during.txt"

run_perf() {
    local name=$1
    shift
    echo "[$(date --iso-8601=seconds)] $name"
    sudo perf stat -x, -e "$EVENTS" -- taskset -c "$CPU" "$@" \
        >"$RESULT_DIR/raw-stdout/$name.txt" \
        2>"$RESULT_DIR/raw-perf/$name.csv"
}

for operation in lookup update; do
    for pair in 1 2 3 4 5; do
        run_perf "jit-array-${operation}-pair${pair}-noop" \
            "$BIN_DIR/array-helper-jit-layers" noop "$operation" 100000 3 "$CPU"
        run_perf "jit-array-${operation}-pair${pair}-real" \
            "$BIN_DIR/array-helper-jit-layers" array "$operation" 100000 3 "$CPU"
    done
done

for operation in lookup update; do
    for pair in 1 2 3 4 5; do
        run_perf "jit-hash-${operation}-pair${pair}-noop" \
            "$BIN_DIR/hash-helper-jit-layers" noop "$operation" 20000 3 "$CPU"
        run_perf "jit-hash-${operation}-pair${pair}-real" \
            "$BIN_DIR/hash-helper-jit-layers" hash "$operation" 20000 3 "$CPU"
    done
done

for operation in lookup update; do
    for layer in control l0 l1 l2 l3; do
        run_perf "direct-array-${operation}-${layer}" \
            "$BIN_DIR/array-map-path-layers" "$layer" "$operation" 100000000 3 "$CPU"
    done
done

for operation in lookup update; do
    for layer in control lock l0 l1 l2 l3; do
        run_perf "direct-hash-${operation}-${layer}" \
            "$BIN_DIR/hash-map-path-layers" "$layer" "$operation" 10000000 3 "$CPU"
    done
done

echo "[$(date --iso-8601=seconds)] completed"
