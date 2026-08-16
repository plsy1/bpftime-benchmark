#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/hash-workload-robustness-arm64-20260816}
BUILD=${BUILD:-$ROOT/build-uprobe-top-arm64-20260803}
CLANG=${CLANG:-/usr/lib/llvm-15/bin/clang}
CC=${CC:-gcc-13}
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
BPFTOOL=${BPFTOOL:-/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool}
CPU=${CPU:-5}
ITERATIONS=${ITERATIONS:-5000}
WARMUP=${WARMUP:-200}
RUNS=${RUNS:-5}
CASES=(empty lookup_control update_control hash_lookup hash_update percpu_hash_lookup percpu_hash_update)

mkdir -p "$OUT/raw/wall"
sudo -n true
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

wait_for_loader() {
	local log=$1
	for _ in $(seq 1 200); do
		grep -q "Successfully started" "$log" && return 0
		kill -0 "$loader_pid" 2>/dev/null || {
			tail -n 100 "$log" >&2
			return 1
		}
		sleep 0.05
	done
	return 1
}

build_config() {
	local tag=$1 active=$2 max_entries=$3 key_size=$4 value_size=$5
	make -C "$ROOT/benchmark/uprobe" hash-workload-sweep-diagnostic \
		HWS_TAG="$tag" HWS_ACTIVE_KEYS="$active" \
		HWS_MAX_ENTRIES="$max_entries" HWS_KEY_SIZE="$key_size" \
		HWS_VALUE_SIZE="$value_size" CLANG="$CLANG" CC="$CC"
}

run_environment() {
	local tag=$1 environment=$2
	local dir="$ROOT/benchmark/uprobe/.output/hash-workload-sweep-$tag"
	local loader="$dir/hash-workload-sweep"
	local victim_rel="benchmark/uprobe/.output/hash-workload-sweep-$tag/hash-workload-sweep-victim"
	local raw="$OUT/raw/wall/$tag/$environment"
	local loader_log="$raw/loader.txt"
	mkdir -p "$raw"

	sudo -n "$TOOL" remove >/dev/null 2>&1 || true
	[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	if [[ $environment == kernel ]]; then
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" "$loader") >"$loader_log" 2>&1 &
	else
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env \
			LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$loader") >"$loader_log" 2>&1 &
	fi
	loader_pid=$!
	wait_for_loader "$loader_log"
	sleep 2

	if [[ $environment == kernel ]]; then
		[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 8 ]]
	else
		[[ -e /dev/shm/bpftime_maps_shm ]]
		[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	fi

	for run in $(seq 1 "$RUNS"); do
		local order=$(( (run - 1) % ${#CASES[@]} ))
		local run_tag="run$(printf '%02d' "$run")"
		local stdout="$raw/victim-$run_tag.txt"
		local timefile="$raw/victim-$run_tag.time.txt"
		cat "/sys/devices/system/cpu/cpu$CPU/cpufreq/scaling_cur_freq" >"$raw/victim-$run_tag.frequency.txt"
		if [[ $environment == kernel ]]; then
			(cd "$ROOT"; sudo -n /usr/bin/time -v -o "$timefile" \
				taskset -c "$CPU" "$victim_rel" "$ITERATIONS" "$order" "$WARMUP") >"$stdout" 2>&1
		else
			(cd "$ROOT"; sudo -n /usr/bin/time -v -o "$timefile" \
				taskset -c "$CPU" env LD_PRELOAD="$AGENT" \
				BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
				"$victim_rel" "$ITERATIONS" "$order" "$WARMUP") >"$stdout" 2>&1
			grep -q "shm_open_type 1" "$stdout"
		fi
		for case_name in "${CASES[@]}"; do
			grep -q "^${case_name}," "$stdout"
		done
	done

	sudo -n kill -TERM "$loader_pid"
	wait "$loader_pid" || true
	loader_pid=
	[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	if [[ $environment == bpftime ]]; then
		sudo -n "$TOOL" remove >/dev/null 2>&1
		[[ ! -e /dev/shm/bpftime_maps_shm ]]
	fi
}

sudo -n jetson_clocks

{
	echo "experiment_start=$(date --iso-8601=ns)"
	echo "source_branch=$(git -C "$ROOT" branch --show-current)"
	echo "source_commit=$(git -C "$ROOT" rev-parse HEAD)"
	echo "source_status_begin"
	git -C "$ROOT" status --short
	echo "source_status_end"
	echo "cpu=$CPU iterations=$ITERATIONS warmup=$WARMUP runs=$RUNS"
	echo "kernel=$(uname -r)"
	echo "machine=$(uname -m)"
	echo "gcc=$($CC --version | head -n 1)"
	echo "clang=$($CLANG --version | head -n 1)"
	echo "cpu_current_khz=$(cat "/sys/devices/system/cpu/cpu$CPU/cpufreq/scaling_cur_freq")"
	echo "cpu_online=$(cat /sys/devices/system/cpu/online)"
	sha256sum "$AGENT" "$SERVER" "$TOOL"
} >"$OUT/environment.txt"

config_index=0
while IFS=, read -r tag family active max_entries key_size value_size; do
	[[ $tag == tag ]] && continue
	build_config "$tag" "$active" "$max_entries" "$key_size" "$value_size"
	dir="$ROOT/benchmark/uprobe/.output/hash-workload-sweep-$tag"
	sha256sum "$dir/hash-workload-sweep" "$dir/hash-workload-sweep-victim" \
		"$dir/hash_workload_sweep.bpf.o" >>"$OUT/environment.txt"
	if (( config_index % 2 == 0 )); then
		run_environment "$tag" kernel
		run_environment "$tag" bpftime
	else
		run_environment "$tag" bpftime
		run_environment "$tag" kernel
	fi
	config_index=$((config_index + 1))
done <"$OUT/configs.csv"

{
	echo "experiment_end=$(date --iso-8601=ns)"
	echo "kernel_links=$(sudo -n "$BPFTOOL" -j link show | jq length)"
	echo "bpftime_shm_exists=$([[ -e /dev/shm/bpftime_maps_shm ]] && echo yes || echo no)"
} >>"$OUT/environment.txt"
