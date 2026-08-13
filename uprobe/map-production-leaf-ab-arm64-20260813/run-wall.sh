#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
DIAG=${DIAG:-/home/jetson/src/.worktrees/map-production-ab-arm64}
BUILD=${BUILD:-$DIAG/build-map-production-ab-arm64}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/map-production-leaf-ab-arm64-20260813}
CPU=${CPU:-5}
RUNS=${RUNS:-5}
ARRAY_ITER=${ARRAY_ITER:-50000}
HASH_ITER=${HASH_ITER:-20000}
WARMUP=${WARMUP:-1000}

AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool

ARRAY_LOADER=$ROOT/benchmark/uprobe/.output/helper-map-closure/helper-map-closure
ARRAY_VICTIM=benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim
HASH_LOADER=$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure
HASH_VICTIM=benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim

MODES=(base cache_control cached_handler direct_map percpu_array_direct percpu_array_fixed_cpu percpu_array_no_copy percpu_hash_lookup_no_find percpu_hash_update_no_find percpu_hash_update_no_copy)

mkdir -p "$OUT/raw/array" "$OUT/raw/hash"
sudo -n true
sudo -n jetson_clocks

loader_pid=
cleanup() {
	set +e
	if [[ -n "$loader_pid" ]]; then
		sudo -n kill -TERM "$loader_pid" 2>/dev/null
		wait "$loader_pid" 2>/dev/null
	fi
	sudo -n "$TOOL" remove >/dev/null 2>&1
	sudo -n chown -R "$(id -u):$(id -g)" "$OUT" 2>/dev/null
}
trap cleanup EXIT INT TERM

wait_started() {
	local log=$1
	for _ in $(seq 1 200); do
		grep -q 'Successfully started' "$log" && return 0
		if ! sudo -n kill -0 "$loader_pid" 2>/dev/null; then
			tail -n 100 "$log" >&2
			return 1
		fi
		sleep 0.1
	done
	tail -n 100 "$log" >&2
	return 1
}

start_loader() {
	local family=$1 loader=$2 log=$OUT/raw/$family/loader.txt
	sudo -n "$TOOL" remove >/dev/null 2>&1 || true
	(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env \
		LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$loader") >"$log" 2>&1 &
	loader_pid=$!
	wait_started "$log"
	grep -q 'Starting syscall server' "$log"
	[[ -e /dev/shm/bpftime_maps_shm ]]
	[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	sleep 5
}

stop_loader() {
	sudo -n kill -TERM "$loader_pid"
	wait "$loader_pid" || true
	loader_pid=
	sudo -n "$TOOL" remove >/dev/null 2>&1
	[[ ! -e /dev/shm/bpftime_maps_shm ]]
}

run_family() {
	local family=$1 loader=$2 victim=$3 iterations=$4
	start_loader "$family" "$loader"
	for run in $(seq 1 "$RUNS"); do
		for offset in $(seq 0 $((${#MODES[@]} - 1))); do
			local index=$(( (run - 1 + offset) % ${#MODES[@]} ))
			local mode=${MODES[$index]}
			local stem=$OUT/raw/$family/$mode-run$(printf '%02d' "$run")
			(cd "$ROOT"; sudo -n /usr/bin/time -v -o "$stem.time.txt" \
				taskset -c "$CPU" env LD_PRELOAD="$AGENT" \
				BPFTIME_MAP_PRODUCTION_AB="$mode" \
				BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
				"$victim" "$iterations" "$run" "$WARMUP") \
				>"$stem.txt" 2>&1
			grep -q 'shm_open_type 1' "$stem.txt"
		done
	done
	stop_loader
}

{
	echo "start=$(date --iso-8601=ns)"
	echo "root=$ROOT"
	echo "root_commit=$(git -C "$ROOT" rev-parse HEAD)"
	echo "diag=$DIAG"
	echo "diag_commit=$(git -C "$DIAG" rev-parse HEAD)"
	echo "diag_status_begin"
	git -C "$DIAG" status --short
	echo "diag_status_end"
	echo "cpu=$CPU runs=$RUNS array_iterations=$ARRAY_ITER hash_iterations=$HASH_ITER warmup=$WARMUP"
	echo "modes=${MODES[*]}"
	echo "cpu_siblings=$(cat /sys/devices/system/cpu/cpu$CPU/topology/thread_siblings_list)"
	echo "cpu_frequency_khz=$(cat /sys/devices/system/cpu/cpu$CPU/cpufreq/scaling_cur_freq)"
	sha256sum "$AGENT" "$SERVER" "$TOOL" "$ARRAY_LOADER" "$ROOT/$ARRAY_VICTIM" "$HASH_LOADER" "$ROOT/$HASH_VICTIM"
} >"$OUT/environment.txt"

run_family array "$ARRAY_LOADER" "$ARRAY_VICTIM" "$ARRAY_ITER"
run_family hash "$HASH_LOADER" "$HASH_VICTIM" "$HASH_ITER"

{
	echo "end=$(date --iso-8601=ns)"
	echo "kernel_links=$(sudo -n "$BPFTOOL" -j link show | jq length)"
	echo "bpftime_shm_exists=$([[ -e /dev/shm/bpftime_maps_shm ]] && echo yes || echo no)"
} >>"$OUT/environment.txt"

python3 "$OUT/summarize.py"
