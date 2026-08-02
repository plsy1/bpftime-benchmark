#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/jetson/src/bpftime-offical-no-btf
BUILD="$ROOT/build-uprobe-top-arm64-20260803"
OUT=/home/jetson/src/benchmark-results/uprobe/uprobe-top-arm64-20260803
RAW="$OUT/raw"
CPU=5
RUNS=5
ITER=100000
EXPECTED_COMMIT=8ed291e130fe3f15f99955b0d259eb119efdaa7d
LOADER="$ROOT/benchmark/uprobe/uprobe"
VICTIM="$ROOT/benchmark/test"
SERVER_SO="$BUILD/runtime/syscall-server/libbpftime-syscall-server.so"
AGENT_SO="$BUILD/runtime/agent/libbpftime-agent.so"
BPFTIMETOOL="$BUILD/tools/bpftimetool/bpftimetool"
BPFTOOL=/usr/lib/linux-tools-6.8.0-134/bpftool

mkdir -p "$RAW"
cd "$ROOT"
sudo -n true

if [[ $(git rev-parse HEAD) != "$EXPECTED_COMMIT" ]]; then
    echo "unexpected source commit: $(git rev-parse HEAD)" >&2
    exit 1
fi
for path in "$LOADER" "$VICTIM" "$SERVER_SO" "$AGENT_SO" "$BPFTIMETOOL" "$BPFTOOL"; do
    if [[ ! -e "$path" ]]; then
        echo "required artifact is missing: $path" >&2
        exit 1
    fi
done
if ! sudo -n nvpmodel -q 2>&1 | grep -q 'MAXN_SUPER'; then
    echo "Jetson is not in MAXN_SUPER mode" >&2
    exit 1
fi

loader_pid=
cleanup() {
    set +e
    if [[ -n "$loader_pid" ]]; then
        sudo -n kill -INT "$loader_pid" 2>/dev/null
        wait "$loader_pid" 2>/dev/null
    fi
    sudo -n "$BPFTIMETOOL" remove >/dev/null 2>&1
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
}
trap cleanup EXIT INT TERM

sudo -n jetson_clocks

{
    echo "Experiment start: $(date --iso-8601=seconds)"
    echo "Host: $(hostname)"
    echo "Kernel: $(uname -a)"
    echo "Architecture: $(uname -m)"
    echo "Source repository: $ROOT"
    echo "Source branch: $(git branch --show-current)"
    echo "Source commit: $(git rev-parse HEAD)"
    echo "Source status before benchmark:"
    git status --short
    echo
    echo "Build directory: $BUILD"
    grep -E '^(CMAKE_BUILD_TYPE|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|BPFTIME_LLVM_JIT|BPFTIME_ENABLE_LTO|ENABLE_PROBE_READ_CHECK|ENABLE_PROBE_WRITE_CHECK):' "$BUILD/CMakeCache.txt"
    /usr/bin/gcc-13 --version | head -1
    /usr/bin/g++-13 --version | head -1
    /usr/lib/llvm-15/bin/clang --version | head -1
    cmake --version | head -1
    echo "Boost include version: $(grep '#define BOOST_LIB_VERSION' /usr/include/boost/version.hpp)"
    echo
    echo "Victim command: sudo taskset -c $CPU $VICTIM 1 $ITER"
    echo "Independent processes per environment: $RUNS"
    echo "Helpers per ordinary map invocation: 1000"
    echo "Loader stabilization delay: 5 seconds"
    echo "Privilege: root for loader and victims"
    echo "Execution: host, no Docker"
    echo
    echo "CPU topology:"
    lscpu -e=CPU,CORE,SOCKET,ONLINE,MAXMHZ,MINMHZ
    echo
    echo "CPU $CPU siblings: $(cat /sys/devices/system/cpu/cpu${CPU}/topology/thread_siblings_list)"
    echo "CPU $CPU governor: $(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_governor)"
    echo "CPU $CPU min kHz: $(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_min_freq)"
    echo "CPU $CPU max kHz: $(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_max_freq)"
    echo "CPU $CPU current kHz: $(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_cur_freq)"
    echo
    sudo -n nvpmodel -q --verbose
    sudo -n jetson_clocks --show
} >"$OUT/environment.txt" 2>&1

wait_started() {
    local log=$1
    for _ in $(seq 1 200); do
        if grep -q 'Successfully started!' "$log"; then
            return 0
        fi
        if [[ -n "$loader_pid" ]] && ! sudo -n kill -0 "$loader_pid" 2>/dev/null; then
            echo "loader exited before startup; see $log" >&2
            return 1
        fi
        sleep 0.1
    done
    echo "timed out waiting for loader; see $log" >&2
    return 1
}

link_count() {
    sudo -n "$BPFTOOL" link show | wc -l
}

record_frequency() {
    local environment=$1
    local run=$2
    {
        echo "timestamp=$(date --iso-8601=ns)"
        echo "cpu${CPU}_current_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_cur_freq)"
        echo "cpu${CPU}_min_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_min_freq)"
        echo "cpu${CPU}_max_khz=$(cat /sys/devices/system/cpu/cpu${CPU}/cpufreq/scaling_max_freq)"
    } >"$RAW/${environment}-run-${run}.frequency.txt"
}

run_victims() {
    local environment=$1
    for run in $(seq 1 "$RUNS"); do
        echo "$(date --iso-8601=seconds) $environment run $run/$RUNS"
        record_frequency "$environment" "$run"
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
            if ! grep -q 'shm_open_type 1' "$RAW/bpftime-run-${run}.txt"; then
                echo "BPFtime run $run did not open shared memory with shm_open_type 1" >&2
                exit 1
            fi
        fi
    done
}

if pgrep -x uprobe >/dev/null; then
    echo "an uprobe loader is already running" >&2
    exit 1
fi
sudo -n "$BPFTIMETOOL" remove >/dev/null 2>&1 || true
if [[ $(link_count) -ne 0 ]]; then
    echo "kernel BPF links already exist before benchmark" >&2
    sudo -n "$BPFTOOL" link show >&2
    exit 1
fi

echo "$(date --iso-8601=seconds) starting kernel loader"
sudo -n taskset -c "$CPU" "$LOADER" >"$RAW/kernel-loader.txt" 2>&1 &
loader_pid=$!
wait_started "$RAW/kernel-loader.txt"
if [[ $(link_count) -eq 0 ]]; then
    echo "kernel loader created no links" >&2
    exit 1
fi
sleep 5
run_victims kernel
sudo -n kill -INT "$loader_pid"
wait "$loader_pid" || true
loader_pid=
if [[ $(link_count) -ne 0 ]]; then
    echo "kernel links remained after loader exit" >&2
    sudo -n "$BPFTOOL" link show >&2
    exit 1
fi

sudo -n "$BPFTIMETOOL" remove >/dev/null 2>&1 || true
echo "$(date --iso-8601=seconds) starting bpftime loader"
sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER_SO" SPDLOG_LEVEL=info \
    "$LOADER" >"$RAW/bpftime-loader.txt" 2>&1 &
loader_pid=$!
wait_started "$RAW/bpftime-loader.txt"
if ! grep -q 'Starting syscall server' "$RAW/bpftime-loader.txt"; then
    echo "BPFtime loader did not start the syscall server" >&2
    exit 1
fi
if [[ ! -e /dev/shm/bpftime_maps_shm ]]; then
    echo "BPFtime shared memory was not created" >&2
    exit 1
fi
if [[ $(link_count) -ne 0 ]]; then
    echo "BPFtime loader unexpectedly created kernel links" >&2
    exit 1
fi
sleep 5
run_victims bpftime
sudo -n kill -INT "$loader_pid"
wait "$loader_pid" || true
loader_pid=
sudo -n "$BPFTIMETOOL" remove >/dev/null 2>&1 || true
if [[ -e /dev/shm/bpftime_maps_shm ]]; then
    echo "BPFtime shared memory remained after cleanup" >&2
    exit 1
fi

{
    echo
    echo "Experiment end: $(date --iso-8601=seconds)"
    echo "Source status after benchmark:"
    git status --short
    echo "Final kernel BPF link count: $(link_count)"
    echo "Final shared memory exists: $([[ -e /dev/shm/bpftime_maps_shm ]] && echo yes || echo no)"
} >>"$OUT/environment.txt"

sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
echo "$(date --iso-8601=seconds) completed"
