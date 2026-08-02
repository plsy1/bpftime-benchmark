#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/y1/src/bpftime-offical-no-btf
BUILD="$ROOT/build-map-path-x64-20260802"
OUT=/home/y1/src/benchmark-results/uprobe/uprobe-top-x64-20260802
RAW="$OUT/raw"
CPU=5
SIBLING=11
RUNS=5
ITER=100000
LOADER="$ROOT/benchmark/uprobe/uprobe"
VICTIM="$ROOT/benchmark/test"
SERVER_SO="$BUILD/runtime/syscall-server/libbpftime-syscall-server.so"
AGENT_SO="$BUILD/runtime/agent/libbpftime-agent.so"
BPFTIMETOOL="$BUILD/tools/bpftimetool/bpftimetool"

mkdir -p "$RAW"
cd "$ROOT"
sudo -n true

turbo_file=/sys/devices/system/cpu/intel_pstate/no_turbo
turbo_before=
sibling_before=$(cat "/sys/devices/system/cpu/cpu${SIBLING}/online")
declare -A governor_before
for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    governor_before["$policy"]=$(cat "$policy/scaling_governor")
done
if [[ -r "$turbo_file" ]]; then
    turbo_before=$(cat "$turbo_file")
fi

loader_pid=
cleanup() {
    set +e
    sudo -n pkill -INT -x uprobe 2>/dev/null
    if [[ -n "$loader_pid" ]]; then
        wait "$loader_pid" 2>/dev/null
    fi
    sudo -n "$BPFTIMETOOL" remove >/dev/null 2>&1
    if [[ "$sibling_before" == 1 ]]; then
        echo 1 | sudo -n tee "/sys/devices/system/cpu/cpu${SIBLING}/online" >/dev/null
    fi
    if [[ -n "$turbo_before" ]]; then
        echo "$turbo_before" | sudo -n tee "$turbo_file" >/dev/null
    fi
    for policy in "${!governor_before[@]}"; do
        echo "${governor_before[$policy]}" | sudo -n tee "$policy/scaling_governor" >/dev/null
    done
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
}
trap cleanup EXIT INT TERM

for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    echo performance | sudo -n tee "$policy/scaling_governor" >/dev/null
done
echo 0 | sudo -n tee "/sys/devices/system/cpu/cpu${SIBLING}/online" >/dev/null
if [[ -w "$turbo_file" || -r "$turbo_file" ]]; then
    echo 1 | sudo -n tee "$turbo_file" >/dev/null
fi

wait_started() {
    local log=$1
    for _ in $(seq 1 200); do
        if grep -q 'Successfully started!' "$log"; then
            return 0
        fi
        if [[ -n "$loader_pid" ]] && ! kill -0 "$loader_pid" 2>/dev/null; then
            echo "loader exited before startup" >&2
            return 1
        fi
        sleep 0.1
    done
    echo "timed out waiting for loader" >&2
    return 1
}

run_victims() {
    local environment=$1
    for run in $(seq 1 "$RUNS"); do
        echo "$(date --iso-8601=seconds) $environment run $run/$RUNS"
        if [[ "$environment" == kernel ]]; then
            sudo -n /usr/bin/time -v -o "$RAW/kernel-run-${run}.time.txt" \
                taskset -c "$CPU" "$VICTIM" 1 "$ITER" \
                >"$RAW/kernel-run-${run}.txt" 2>&1
        else
            sudo -n /usr/bin/time -v -o "$RAW/bpftime-run-${run}.time.txt" \
                taskset -c "$CPU" env LD_PRELOAD="$AGENT_SO" \
                BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
                "$VICTIM" 1 "$ITER" \
                >"$RAW/bpftime-run-${run}.txt" 2>&1
        fi
    done
}

sudo -n pkill -INT -x uprobe 2>/dev/null || true
sudo -n "$BPFTIMETOOL" remove >/dev/null 2>&1 || true

echo "$(date --iso-8601=seconds) starting kernel loader"
sudo -n taskset -c "$CPU" "$LOADER" >"$RAW/kernel-loader.txt" 2>&1 &
loader_pid=$!
wait_started "$RAW/kernel-loader.txt"
if [[ $(sudo -n bpftool link show | wc -l) -eq 0 ]]; then
    echo "kernel loader created no links" >&2
    exit 1
fi
sleep 5
run_victims kernel
sudo -n pkill -INT -x uprobe
wait "$loader_pid" || true
loader_pid=
if [[ $(sudo -n bpftool link show | wc -l) -ne 0 ]]; then
    echo "kernel links remained after loader exit" >&2
    exit 1
fi

sudo -n "$BPFTIMETOOL" remove >/dev/null 2>&1 || true
echo "$(date --iso-8601=seconds) starting bpftime loader"
sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER_SO" SPDLOG_LEVEL=info \
    "$LOADER" >"$RAW/bpftime-loader.txt" 2>&1 &
loader_pid=$!
wait_started "$RAW/bpftime-loader.txt"
if [[ ! -e /dev/shm/bpftime_maps_shm ]]; then
    echo "bpftime shared memory was not created" >&2
    exit 1
fi
if [[ $(sudo -n bpftool link show | wc -l) -ne 0 ]]; then
    echo "bpftime loader unexpectedly created kernel links" >&2
    exit 1
fi
sleep 5
run_victims bpftime
sudo -n pkill -INT -x uprobe
wait "$loader_pid" || true
loader_pid=
sudo -n "$BPFTIMETOOL" remove >/dev/null 2>&1 || true

echo "$(date --iso-8601=seconds) completed"
