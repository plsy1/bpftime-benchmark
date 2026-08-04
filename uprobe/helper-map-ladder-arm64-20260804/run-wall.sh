#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/helper-map-ladder-arm64-20260804}
BUILD=${BUILD:-$ROOT/build-uprobe-top-arm64-20260803}
LOADER=$ROOT/benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder
VICTIM_REL=benchmark/uprobe/.output/helper-map-ladder/helper-map-ladder-victim
VICTIM=$ROOT/$VICTIM_REL
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool
CPU=${CPU:-5}
ITERATIONS=${ITERATIONS:-100000}
WARMUP=${WARMUP:-1000}
RUNS=${RUNS:-6}
EXPECTED_COMMIT=${EXPECTED_COMMIT:-47f853ccfa86e054ff033e734dadd17f2d60166c}

CASES=(empty loop_control simple_helper array_lookup hash_hit hash_miss)

mkdir -p "$OUT/raw/kernel" "$OUT/raw/bpftime"
sudo -n true
[[ $(git -C "$ROOT" rev-parse HEAD) == "$EXPECTED_COMMIT" ]]
[[ -x "$LOADER" && -x "$VICTIM" && -x "$AGENT" && -x "$SERVER" && -x "$TOOL" ]]

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

sudo -n jetson_clocks
{
	echo "experiment_start=$(date --iso-8601=ns)"
	echo "host=$(hostname)"
	echo "uname=$(uname -a)"
	echo "source_branch=$(git -C "$ROOT" branch --show-current)"
	echo "source_commit=$(git -C "$ROOT" rev-parse HEAD)"
	echo "source_status_begin:"
	git -C "$ROOT" status --short
	echo "cpu=$CPU"
	echo "iterations=$ITERATIONS"
	echo "warmup=$WARMUP"
	echo "independent_processes=$RUNS"
	echo "cases=${CASES[*]}"
	echo "cpu_siblings=$(cat /sys/devices/system/cpu/cpu$CPU/topology/thread_siblings_list)"
	echo "cpu_min_khz=$(cat /sys/devices/system/cpu/cpu$CPU/cpufreq/scaling_min_freq)"
	echo "cpu_max_khz=$(cat /sys/devices/system/cpu/cpu$CPU/cpufreq/scaling_max_freq)"
	echo "cpu_current_khz=$(cat /sys/devices/system/cpu/cpu$CPU/cpufreq/scaling_cur_freq)"
	sudo -n nvpmodel -q
	sudo -n jetson_clocks --show
	/usr/lib/llvm-15/bin/clang --version | head -1
	/usr/bin/gcc-13 --version | head -1
	sha256sum "$LOADER" "$VICTIM" "$AGENT" "$SERVER" \
		"$ROOT/benchmark/uprobe/.output/helper-map-ladder/helper_map_ladder.bpf.o"
} >"$OUT/environment.txt" 2>&1

wait_for_loader() {
	local log=$1
	for _ in $(seq 1 200); do
		grep -q "Successfully started!" "$log" && return 0
		kill -0 "$loader_pid" 2>/dev/null || { cat "$log" >&2; return 1; }
		sleep 0.05
	done
	cat "$log" >&2
	return 1
}

stop_loader() {
	sudo -n kill -TERM "$loader_pid"
	wait "$loader_pid" || true
	loader_pid=
}

run_environment() {
	local environment=$1
	local raw=$OUT/raw/$environment
	local loader_log=$raw/loader.txt

	if [[ $environment == kernel ]]; then
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" "$LOADER") \
			>"$loader_log" 2>&1 &
	else
		sudo -n "$TOOL" remove >/dev/null 2>&1
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env \
			LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$LOADER") \
			>"$loader_log" 2>&1 &
	fi
	loader_pid=$!
	wait_for_loader "$loader_log"
	sleep 5

	if [[ $environment == kernel ]]; then
		[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 7 ]]
	else
		[[ -e /dev/shm/bpftime_maps_shm ]]
		[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	fi

	for run in $(seq 1 "$RUNS"); do
		local order=$(( (run - 1) % 6 ))
		local tag=run$(printf '%02d' "$run")
		local stdout=$raw/victim-$tag.txt
		local timefile=$raw/victim-$tag.time.txt
		if [[ $environment == kernel ]]; then
			(cd "$ROOT"; sudo -n /usr/bin/time -v -o "$timefile" \
				taskset -c "$CPU" "$VICTIM_REL" "$ITERATIONS" \
				"$order" "$WARMUP") >"$stdout" 2>&1
		else
			(cd "$ROOT"; sudo -n /usr/bin/time -v -o "$timefile" \
				taskset -c "$CPU" env LD_PRELOAD="$AGENT" \
				BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info "$VICTIM_REL" \
				"$ITERATIONS" "$order" "$WARMUP") >"$stdout" 2>&1
			grep -q "shm_open_type 1" "$stdout"
		fi
		for case_name in "${CASES[@]}"; do
			grep -q "^${case_name}," "$stdout"
		done
		[[ $(grep -Ec '^(empty|loop_control|simple_helper|array_lookup|hash_hit|hash_miss),' "$stdout") -eq 6 ]]
		if [[ $environment == kernel && $run -eq 1 ]]; then
			for map_name in ladder_hash ladder_array; do
				map_id=$(sudo -n "$BPFTOOL" -j map show | \
					jq -r --arg name "$map_name" '.[] | select(.name == $name) | .id')
				[[ -n "$map_id" ]]
				if [[ $map_name == ladder_hash ]]; then
					[[ $(sudo -n "$BPFTOOL" -j map dump id "$map_id" | jq length) -eq 1000 ]]
				fi
			done
			echo "ladder_hash_entries=1000" >"$raw/map-validation.txt"
			echo "ladder_array_present=yes" >>"$raw/map-validation.txt"
		fi
	done

	stop_loader
	[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	if [[ $environment == bpftime ]]; then
		sudo -n "$TOOL" remove >/dev/null 2>&1
		[[ ! -e /dev/shm/bpftime_maps_shm ]]
	fi
}

run_environment kernel
run_environment bpftime

{
	echo "experiment_end=$(date --iso-8601=ns)"
	echo "kernel_links=$(sudo -n "$BPFTOOL" -j link show | jq length)"
	echo "bpf_stats=$(sysctl -n kernel.bpf_stats_enabled)"
	echo "bpftime_shm_exists=$([[ -e /dev/shm/bpftime_maps_shm ]] && echo yes || echo no)"
} >>"$OUT/environment.txt"

sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
