#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/top-hash-residual-arm64-20260803}
BUILD=$ROOT/build-uprobe-top-arm64-20260803
LOADER=$ROOT/benchmark/uprobe/.output/top-hash-residual/top-hash-residual
VICTIM_REL=benchmark/uprobe/.output/top-hash-residual/top-hash-residual-victim
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
BPFTOOL=/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool
PERF=/usr/lib/linux-tools-6.8.0-134/perf
CPU=5
ITERATIONS=100000
WARMUP=1000
ROUNDS=3

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
	sudo -n pkill -TERM -f "$LOADER" 2>/dev/null
	sudo -n "$TOOL" remove >/dev/null 2>&1
	sudo -n chown -R "$(id -u):$(id -g)" "$OUT" 2>/dev/null
}
trap cleanup EXIT INT TERM

wait_for_loader() {
	local log=$1
	for _ in $(seq 1 200); do
		grep -q "Successfully started!" "$log" && return 0
		kill -0 "$loader_pid" 2>/dev/null || return 1
		sleep 0.05
	done
	return 1
}

start_loader() {
	local environment=$1
	local log=$OUT/raw-pmu/$environment/loader.txt
	if [[ $environment == kernel ]]; then
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" "$LOADER") \
			>"$log" 2>&1 &
	else
		sudo -n "$TOOL" remove >/dev/null 2>&1
		(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env \
			LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$LOADER") \
			>"$log" 2>&1 &
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

sudo -n jetson_clocks
for environment in kernel bpftime; do
	start_loader "$environment"
	for metric in cycles instructions; do
		for round in $(seq 1 "$ROUNDS"); do
			for case_name in empty loop_control hash_lookup; do
				base=$OUT/raw-pmu/$environment/${metric}-${case_name}-run$(printf '%02d' "$round")
				if [[ $environment == kernel ]]; then
					(cd "$ROOT"; sudo -n taskset -c "$CPU" "$PERF" stat \
						--no-big-num -x, -e "$metric" -o "$base.perf.csv" -- \
						"$VICTIM_REL" "$ITERATIONS" 0 "$WARMUP" "$case_name") \
						>"$base.stdout.txt" 2>"$base.stderr.txt"
				else
					(cd "$ROOT"; sudo -n taskset -c "$CPU" "$PERF" stat \
						--no-big-num -x, -e "$metric" -o "$base.perf.csv" -- env \
						LD_PRELOAD="$AGENT" BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
						"$VICTIM_REL" "$ITERATIONS" 0 "$WARMUP" "$case_name") \
						>"$base.stdout.txt" 2>"$base.stderr.txt"
					grep -q "shm_open_type 1" "$base.stdout.txt"
				fi
				[[ $(grep -Ec "^${case_name}," "$base.stdout.txt") -eq 1 ]]
				grep -q ",$metric," "$base.perf.csv"
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
