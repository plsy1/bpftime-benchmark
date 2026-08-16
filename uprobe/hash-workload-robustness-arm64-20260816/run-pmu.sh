#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/hash-workload-robustness-arm64-20260816}
BUILD=${BUILD:-$ROOT/build-uprobe-top-arm64-20260803}
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
BPFTOOL=${BPFTOOL:-/home/jetson/src/.toolchains/bpftool-profile-arm64/bpftool}
PERF=${PERF:-/usr/lib/linux-tools-6.8.0-134/perf}
CPU=${CPU:-5}
ITERATIONS=${ITERATIONS:-5000}
WARMUP=${WARMUP:-50}
RUNS=${RUNS:-3}
METRICS=(cycles instructions)

mkdir -p "$OUT/raw/pmu"
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

run_victim() {
	local environment=$1 victim_rel=$2 only=$3 perf_file=$4 stdout=$5 metric=$6 order=$7
	if [[ $environment == kernel ]]; then
		(cd "$ROOT"; sudo -n "$PERF" stat -x, -o "$perf_file" -e "$metric" -- \
			taskset -c "$CPU" "$victim_rel" "$ITERATIONS" "$order" "$WARMUP" "$only") >"$stdout" 2>&1
	else
		(cd "$ROOT"; sudo -n "$PERF" stat -x, -o "$perf_file" -e "$metric" -- \
			taskset -c "$CPU" env LD_PRELOAD="$AGENT" \
			BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
			"$victim_rel" "$ITERATIONS" "$order" "$WARMUP" "$only") >"$stdout" 2>&1
		grep -q "shm_open_type 1" "$stdout"
	fi
	grep -q "^${only}," "$stdout"
	grep -q ",$metric," "$perf_file"
}

run_environment() {
	local tag=$1 environment=$2
	local dir="$ROOT/benchmark/uprobe/.output/hash-workload-sweep-$tag"
	local loader="$dir/hash-workload-sweep"
	local victim_rel="benchmark/uprobe/.output/hash-workload-sweep-$tag/hash-workload-sweep-victim"
	local raw="$OUT/raw/pmu/$tag/$environment"
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

	while IFS=, read -r endpoint_tag case_name control reason; do
		[[ $endpoint_tag == "tag" ]] && continue
		[[ $endpoint_tag != "$tag" ]] && continue
		for metric in "${METRICS[@]}"; do
			for run in $(seq 1 "$RUNS"); do
				local stem="$raw/${metric}-${case_name}-run$(printf '%02d' "$run")"
				local order=$(( (run - 1) % 7 ))
				if (( run % 2 == 1 )); then
					run_victim "$environment" "$victim_rel" "$control" \
						"$stem-control.perf.csv" "$stem-control.stdout" "$metric" "$order"
					run_victim "$environment" "$victim_rel" "$case_name" \
						"$stem-real.perf.csv" "$stem-real.stdout" "$metric" "$order"
				else
					run_victim "$environment" "$victim_rel" "$case_name" \
						"$stem-real.perf.csv" "$stem-real.stdout" "$metric" "$order"
					run_victim "$environment" "$victim_rel" "$control" \
						"$stem-control.perf.csv" "$stem-control.stdout" "$metric" "$order"
				fi
			done
		done
	done <"$OUT/pmu-endpoints.csv"

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
mapfile -t TAGS < <(tail -n +2 "$OUT/pmu-endpoints.csv" | cut -d, -f1 | awk '!seen[$0]++')
tag_index=0
for tag in "${TAGS[@]}"; do
	if (( tag_index % 2 == 0 )); then
		run_environment "$tag" kernel
		run_environment "$tag" bpftime
	else
		run_environment "$tag" bpftime
		run_environment "$tag" kernel
	fi
	tag_index=$((tag_index + 1))
done

{
	echo "pmu_end=$(date --iso-8601=ns)"
	echo "perf=$($PERF --version)"
	echo "pmu_iterations=$ITERATIONS pmu_warmup=$WARMUP pmu_runs=$RUNS"
	echo "kernel_links_after_pmu=$(sudo -n "$BPFTOOL" -j link show | jq length)"
	echo "bpftime_shm_after_pmu=$([[ -e /dev/shm/bpftime_maps_shm ]] && echo yes || echo no)"
} >>"$OUT/environment.txt"
