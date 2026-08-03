#!/usr/bin/env bash
set -euo pipefail

METRIC=${1:?usage: profile-arm64.sh <cycles|instructions|l1d_loads|llc_misses>}
case "$METRIC" in
    cycles|instructions|l1d_loads|llc_misses) ;;
    *) echo "unsupported metric: $METRIC" >&2; exit 2 ;;
esac

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/kernel-array-update-sizes-arm64-20260803}
RAW="$OUT/raw-profile-$METRIC-pairwise"
CPU=${CPU:-5}
ITERATIONS=${ITERATIONS:-20000}
ROUNDS=${ROUNDS:-5}
WARMUP=${WARMUP:-1000}
EXPECTED_RUNS=$((ITERATIONS * ROUNDS + WARMUP))
PROFILE_SECONDS=${PROFILE_SECONDS:-10}
EXPECTED_COMMIT=176eb291ead95eb5f8a56280deae626fac46eaa9
LOADER="$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes"
VICTIM="$ROOT/benchmark/uprobe/.output/kernel-array-update-sizes/kernel-array-update-sizes-victim"
BPFTOOL=${BPFTOOL:-/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool}
SIZES=(8 16 32 64 128 256)

mkdir -p "$RAW"
sudo -n true
[[ $(git -C "$ROOT" rev-parse HEAD) == "$EXPECTED_COMMIT" ]]
sudo -n nvpmodel -q 2>&1 | grep -q MAXN_SUPER
[[ -x "$BPFTOOL" ]]

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
    for size in "${SIZES[@]}"; do
        unlink "/tmp/kaus-${METRIC}-${size}-$$.gate" 2>/dev/null || true
    done
    sudo -n sysctl -q -w "kernel.bpf_stats_enabled=$bpf_stats_before"
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
}
trap cleanup EXIT INT TERM

sudo -n jetson_clocks
sudo -n sysctl -q -w kernel.bpf_stats_enabled=1
for size in "${SIZES[@]}"; do
    gate="/tmp/kaus-${METRIC}-${size}-$$.gate"
    before_max=$(sudo -n "$BPFTOOL" -j prog show | jq 'map(.id) | max // 0')
    sudo -n taskset -c "$CPU" env KAUS_START_GATE="$gate" \
        KAUS_ONLY_SIZE="$size" \
        "$LOADER" "$VICTIM" "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
        >"$RAW/${size}-raw.csv" 2>"$RAW/${size}-loader.stderr.txt" &
    loader_pid=$!

    declare -A ids=()
    for _ in $(seq 1 500); do
        programs=$(sudo -n "$BPFTOOL" -j prog show)
        for impl in ctrl real; do
            name="kaus_${size}_${impl}"
            id=$(jq -r --argjson base "$before_max" --arg name "$name" \
                '.[] | select(.id > $base and .name == $name) | .id' \
                <<<"$programs")
            [[ -n "$id" ]] && ids[$impl]=$id
        done
        [[ ${#ids[@]} -eq 2 ]] && break
        sudo -n kill -0 "$loader_pid" 2>/dev/null || {
            echo "loader exited before ${size}B programs were found" >&2
            exit 1
        }
        sleep 0.01
    done
    [[ ${#ids[@]} -eq 2 ]]

    for impl in ctrl real; do
        key="${size}_${impl}"
        id=${ids[$impl]}
        printf '%s,%s,%s\n' "$size" "$impl" "$id" >>"$RAW/program-ids.csv"
        sudo -n "$BPFTOOL" prog dump xlated id "$id" \
            >"$RAW/${key}-xlated.txt"
        sudo -n "$BPFTOOL" prog dump jited id "$id" \
            >"$RAW/${key}-jited.txt" 2>"$RAW/${key}-jited.stderr.txt" || true
        sudo -n "$BPFTOOL" -j prog profile id "$id" \
            duration "$PROFILE_SECONDS" "$METRIC" \
            >"$RAW/${key}.json" 2>"$RAW/${key}.stderr.txt" &
        profile_pids+=("$!")
    done

    sleep 1
    touch "$gate"
    wait "$loader_pid"
    loader_pid=
    for pid in "${profile_pids[@]}"; do
        wait "$pid"
    done
    profile_pids=()
    unlink "$gate"

    for impl in ctrl real; do
        json="$RAW/${size}_${impl}.json"
        jq -e --arg metric "$METRIC" --argjson runs "$EXPECTED_RUNS" \
            'type == "array" and length == 1 and
             .[0].metric == $metric and .[0].run_cnt == $runs and
             .[0].enabled == .[0].running' "$json" >/dev/null
    done
done

if [[ $(sudo -n "$BPFTOOL" link show | wc -l) -ne 0 ]]; then
    echo "BPF links remained after profile run" >&2
    exit 1
fi
