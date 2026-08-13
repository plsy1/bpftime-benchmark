#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/helper-map-closure-arm64-20260812}
BUILD=${BUILD:-$ROOT/build-uprobe-top-arm64-20260803}
LOADER=$ROOT/benchmark/uprobe/.output/helper-map-closure/helper-map-closure
VICTIM_REL=benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool
PERF=/usr/lib/linux-tools-6.8.0-134/perf
CPU=${CPU:-5}
ITERATIONS=${ITERATIONS:-20000}
WARMUP=${WARMUP:-1000}
ROUNDS=${ROUNDS:-3}
SPECS=(array_lookup:lookup_control percpu_array_lookup:lookup_control array_update:update_control percpu_array_update:update_control)

mkdir -p "$OUT/raw-pmu/kernel" "$OUT/raw-pmu/bpftime"
sudo -n true
[[ -x "$PERF" && -x "$LOADER" && -x "$AGENT" && -x "$SERVER" ]]

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
		kill -0 "$loader_pid" 2>/dev/null || { tail -n 100 "$log" >&2; return 1; }
		sleep 0.05
	done
	tail -n 100 "$log" >&2
	return 1
}

start_loader() {
	local environment=$1
	local log=$OUT/raw-pmu/$environment/loader.txt
	if [[ $environment == kernel ]]; then
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" "$LOADER") >"$log" 2>&1 &
	else
		sudo -n "$TOOL" remove >/dev/null 2>&1
		sleep 1
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$LOADER") >"$log" 2>&1 &
	fi
	loader_pid=$!
	wait_for_loader "$log"
	sleep 5
}

stop_loader() {
	sudo -n kill -TERM "$loader_pid"
	wait "$loader_pid" || true
	loader_pid=
}

run_one() {
	local environment=$1 metric=$2 case_name=$3 role=$4 round=$5
	local raw=$OUT/raw-pmu/$environment
	local base=$raw/${metric}-${case_name}-run$(printf '%02d' "$round")-${role}
	local selected=$case_name
	[[ $role == control ]] && selected=$6
	if [[ $environment == kernel ]]; then
		(cd "$ROOT"; sudo -n taskset -c "$CPU" "$PERF" stat --no-big-num -x, -e "$metric" -o "$base.perf.csv" -- "$VICTIM_REL" "$ITERATIONS" 0 "$WARMUP" "$selected") >"$base.stdout.txt" 2>"$base.stderr.txt"
	else
		(cd "$ROOT"; sudo -n taskset -c "$CPU" "$PERF" stat --no-big-num -x, -e "$metric" -o "$base.perf.csv" -- env LD_PRELOAD="$AGENT" BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info "$VICTIM_REL" "$ITERATIONS" 0 "$WARMUP" "$selected") >"$base.stdout.txt" 2>"$base.stderr.txt"
		grep -q "shm_open_type 1" "$base.stdout.txt"
	fi
	grep -q "^${selected}," "$base.stdout.txt"
	grep -q ",$metric," "$base.perf.csv"
}

sudo -n jetson_clocks
for environment in ${MODES:-kernel bpftime}; do
	start_loader "$environment"
	for metric in cycles instructions; do
		for round in $(seq 1 "$ROUNDS"); do
			for spec in "${SPECS[@]}"; do
				case_name=${spec%%:*}
				control=${spec##*:}
				if (( round % 2 )); then
					run_one "$environment" "$metric" "$case_name" control "$round" "$control"
					run_one "$environment" "$metric" "$case_name" real "$round" "$control"
				else
					run_one "$environment" "$metric" "$case_name" real "$round" "$control"
					run_one "$environment" "$metric" "$case_name" control "$round" "$control"
				fi
			done
		done
	done
	stop_loader
	[[ $(sudo -n "$BPFTOOL" -j link show | jq length) -eq 0 ]]
	if [[ $environment == bpftime ]]; then
		sudo -n "$TOOL" remove >/dev/null 2>&1
		[[ ! -e /dev/shm/bpftime_maps_shm ]]
	fi
done

sudo -n chown -R "$(id -u):$(id -g)" "$OUT"
