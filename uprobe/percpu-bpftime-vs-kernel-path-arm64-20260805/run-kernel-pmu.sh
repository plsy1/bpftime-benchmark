#!/usr/bin/env bash
set -euo pipefail

SRC=/home/jetson/src/bpftime-offical-no-btf
OUT=/home/jetson/src/benchmark-results/uprobe/percpu-bpftime-vs-kernel-path-arm64-20260805
KPMR="$SRC/benchmark/uprobe/.output/kernel-percpu-map-runtime/kernel-percpu-map-runtime"
VICTIM="$SRC/benchmark/uprobe/.output/kernel-percpu-map-runtime/kernel-percpu-map-runtime-victim"
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool
CPU=5
ITERATIONS=20000
ROUNDS=80
WARMUP=1000
DURATION=8

test "$(git -C "$SRC" rev-parse HEAD)" = "e0240a1a81c461f758d3db9fcb8d159e2d9dcf98"
test -x "$KPMR" -a -x "$VICTIM" -a -x "$BPFTOOL"
sudo -n true

names=(kpa_l_ctl kpa_l_real kpa_u_ctl kpa_u_real
       kpp_l_ctl kpp_l_real kpp_u_ctl kpp_u_real
       kha_l_ctl kha_l_real kha_u_ctl kha_u_real
       khp_l_ctl khp_l_real khp_u_ctl khp_u_real)

for metric in cycles instructions; do
    RAW="$OUT/raw-perf/kernel-$metric"
    mkdir -p "$RAW"
    OLD_STATS=$(sysctl -n kernel.bpf_stats_enabled)
    LOADER_PID=
    PROFILE_PIDS=()
    cleanup() {
        set +e
        for pid in "${PROFILE_PIDS[@]}"; do
            sudo -n kill "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null
        done
        if [[ -n "$LOADER_PID" ]]; then
            sudo -n kill "$LOADER_PID" 2>/dev/null
            wait "$LOADER_PID" 2>/dev/null
        fi
        sudo -n sysctl -q -w "kernel.bpf_stats_enabled=$OLD_STATS"
    }
    trap cleanup EXIT INT TERM

    sudo -n jetson_clocks
    sudo -n sysctl -q -w kernel.bpf_stats_enabled=1
    sudo -n taskset -c "$CPU" "$KPMR" "$VICTIM" \
        "$ITERATIONS" "$ROUNDS" "$CPU" "$WARMUP" \
        >"$RAW/loader.csv" 2>"$RAW/loader.stderr.txt" &
    LOADER_PID=$!

    ids_json=
    for _ in $(seq 1 500); do
        ids_json=$(sudo -n "$BPFTOOL" -j prog show)
        found=1
        for name in "${names[@]}"; do
            id=$(jq -r --arg name "$name" '.[] | select(.name == $name) | .id' <<<"$ids_json" | tail -n 1)
            if [[ -z "$id" ]]; then found=0; break; fi
        done
        if ((found)); then break; fi
        if ! kill -0 "$LOADER_PID" 2>/dev/null; then
            echo "kernel loader exited before all programs were visible" >&2
            exit 1
        fi
        sleep 0.01
    done

    : >"$RAW/program-ids.txt"
    for name in "${names[@]}"; do
        id=$(jq -r --arg name "$name" '.[] | select(.name == $name) | .id' <<<"$ids_json" | tail -n 1)
        printf '%s=%s\n' "$name" "$id" >>"$RAW/program-ids.txt"
        sudo -n "$BPFTOOL" prog dump xlated id "$id" >"$RAW/$name.xlated.txt"
        sudo -n "$BPFTOOL" prog dump jited id "$id" >"$RAW/$name.jited.txt" \
            2>"$RAW/$name.jited.stderr.txt" || true
        # Profile one BPF program at a time.  Profiling all 16 programs
        # concurrently multiplexes the single PMU event and makes
        # enabled/running differ, so the result cannot be compared directly.
        sudo -n "$BPFTOOL" -j prog profile id "$id" duration "$DURATION" "$metric" \
            >"$RAW/$name.profile.json" 2>"$RAW/$name.profile.stderr.txt"
    done

    wait "$LOADER_PID"
    LOADER_PID=

    for name in "${names[@]}"; do
        jq -e --arg metric "$metric" \
            'type == "array" and length == 1 and .[0].metric == $metric and
             .[0].run_cnt > 0 and .[0].enabled == .[0].running' \
            "$RAW/$name.profile.json" >/dev/null
    done
    trap - EXIT INT TERM
    cleanup
done
