#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/helper-hash-closure-arm64-20260812}
BUILD=${BUILD:-$ROOT/build-uprobe-top-arm64-20260803}
LOADER=$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure
VICTIM_REL=benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool
CPU=${CPU:-5}
ITERATIONS=${ITERATIONS:-20000}
WARMUP=${WARMUP:-1000}
RUNS=${RUNS:-5}
CASES=(empty lookup_control update_control hash_lookup hash_update percpu_hash_lookup percpu_hash_update)

mkdir -p "$OUT/raw/kernel" "$OUT/raw/bpftime"
sudo -n true
loader_pid=
cleanup() {
	set +e
	if [[ -n "$loader_pid" ]]; then sudo -n kill -TERM "$loader_pid" 2>/dev/null; wait "$loader_pid" 2>/dev/null; fi
	sudo -n "$TOOL" remove >/dev/null 2>&1
	sudo -n chown -R "$(id -u):$(id -g)" "$OUT" 2>/dev/null
}
trap cleanup EXIT INT TERM

sudo -n jetson_clocks
{
	echo "experiment_start=$(date --iso-8601=ns)"
	echo "source_branch=$(git -C "$ROOT" branch --show-current)"
	echo "source_base_commit=$(git -C "$ROOT" rev-parse HEAD)"
	echo "cpu=$CPU iterations=$ITERATIONS warmup=$WARMUP runs=$RUNS"
	echo "cpu_current_khz=$(cat /sys/devices/system/cpu/cpu$CPU/cpufreq/scaling_cur_freq)"
	sha256sum "$LOADER" "$ROOT/$VICTIM_REL" "$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper_hash_closure.bpf.o" "$AGENT" "$SERVER"
} >"$OUT/environment.txt"

wait_for_loader() {
	local log=$1
	for _ in $(seq 1 200); do
		grep -q "Successfully started" "$log" && return 0
		kill -0 "$loader_pid" 2>/dev/null || { tail -n 100 "$log" >&2; return 1; }
		sleep 0.05
	done
	return 1
}

run_environment() {
	local environment=$1 raw=$OUT/raw/$1 loader_log=$OUT/raw/$1/loader.txt
	if [[ $environment == kernel ]]; then
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" "$LOADER") >"$loader_log" 2>&1 &
	else
		sudo -n "$TOOL" remove >/dev/null 2>&1
		sleep 1
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$LOADER") >"$loader_log" 2>&1 &
	fi
	loader_pid=$!
	wait_for_loader "$loader_log"
	sleep 5
	if [[ $environment == kernel ]]; then
		[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 8 ]]
	else
		[[ -e /dev/shm/bpftime_maps_shm ]]
		[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	fi
	for run in $(seq 1 "$RUNS"); do
		order=$(( (run - 1) % ${#CASES[@]} ))
		tag=run$(printf '%02d' "$run")
		stdout=$raw/victim-$tag.txt
		timefile=$raw/victim-$tag.time.txt
		if [[ $environment == kernel ]]; then
			(cd "$ROOT"; sudo -n /usr/bin/time -v -o "$timefile" taskset -c "$CPU" "$VICTIM_REL" "$ITERATIONS" "$order" "$WARMUP") >"$stdout" 2>&1
		else
			(cd "$ROOT"; sudo -n /usr/bin/time -v -o "$timefile" taskset -c "$CPU" env LD_PRELOAD="$AGENT" BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info "$VICTIM_REL" "$ITERATIONS" "$order" "$WARMUP") >"$stdout" 2>&1
			grep -q "shm_open_type 1" "$stdout"
		fi
		for case_name in "${CASES[@]}"; do grep -q "^${case_name}," "$stdout"; done
	done
	sudo -n kill -TERM "$loader_pid"
	wait "$loader_pid" || true
	loader_pid=
	[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	if [[ $environment == bpftime ]]; then sudo -n "$TOOL" remove >/dev/null 2>&1; [[ ! -e /dev/shm/bpftime_maps_shm ]]; fi
}

for environment in ${MODES:-kernel bpftime}; do run_environment "$environment"; done
{
	echo "experiment_end=$(date --iso-8601=ns)"
	echo "kernel_links=$(sudo -n "$BPFTOOL" -j link show | jq length)"
	echo "bpftime_shm_exists=$([[ -e /dev/shm/bpftime_maps_shm ]] && echo yes || echo no)"
} >>"$OUT/environment.txt"
