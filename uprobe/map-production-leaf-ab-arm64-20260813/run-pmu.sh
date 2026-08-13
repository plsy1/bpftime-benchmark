#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
DIAG=${DIAG:-/home/jetson/src/.worktrees/map-production-ab-arm64}
BUILD=${BUILD:-$DIAG/build-map-production-ab-arm64}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/map-production-leaf-ab-arm64-20260813}
CPU=${CPU:-5}
RUNS=${RUNS:-3}
ITER=${ITER:-20000}
WARMUP=${WARMUP:-1000}
PERF=${PERF:-/usr/lib/linux-tools-6.8.0-134/perf}

AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
ARRAY_LOADER=$ROOT/benchmark/uprobe/.output/helper-map-closure/helper-map-closure
ARRAY_VICTIM=benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim
HASH_LOADER=$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure
HASH_VICTIM=benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim

ARRAY_SPECS=(
  'array_lookup:cache_control,cached_handler,direct_map'
  'array_update:cache_control,cached_handler,direct_map'
  'percpu_array_lookup:base,cache_control,cached_handler,direct_map,percpu_array_direct,percpu_array_fixed_cpu'
  'percpu_array_update:base,cache_control,cached_handler,direct_map,percpu_array_direct,percpu_array_fixed_cpu,percpu_array_no_copy'
)
HASH_SPECS=(
  'percpu_hash_lookup:base,cache_control,cached_handler,direct_map,percpu_hash_lookup_no_find'
  'percpu_hash_update:base,cache_control,cached_handler,direct_map,percpu_hash_update_no_copy,percpu_hash_update_no_find'
)

mkdir -p "$OUT/raw/pmu/array" "$OUT/raw/pmu/hash"
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
  sudo -n chown -R "$(id -u):$(id -g)" "$OUT/raw/pmu" 2>/dev/null
}
trap cleanup EXIT INT TERM

start_loader() {
  local family=$1 loader=$2 log=$OUT/raw/pmu/$family/loader.txt
  sudo -n "$TOOL" remove >/dev/null 2>&1 || true
  (cd "$ROOT"; exec sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER" \
    SPDLOG_LEVEL=info "$loader") >"$log" 2>&1 &
  loader_pid=$!
  for _ in $(seq 1 200); do
    grep -q 'Successfully started' "$log" && break
    sudo -n kill -0 "$loader_pid" 2>/dev/null || { tail -100 "$log"; return 1; }
    sleep 0.1
  done
  grep -q 'Starting syscall server' "$log"
  grep -q 'Successfully started' "$log"
  sleep 5
}

stop_loader() {
  sudo -n kill -TERM "$loader_pid"
  wait "$loader_pid" || true
  loader_pid=
  sudo -n "$TOOL" remove >/dev/null 2>&1
}

run_family() {
  local family=$1 loader=$2 victim=$3 specs_name=$4
  local -n specs=$specs_name
  start_loader "$family" "$loader"
  for run in $(seq 1 "$RUNS"); do
    for spec in "${specs[@]}"; do
      local case_name=${spec%%:*} modes_csv=${spec#*:}
      IFS=, read -ra modes <<<"$modes_csv"
      for offset in $(seq 0 $((${#modes[@]} - 1))); do
        local index=$(( (run - 1 + offset) % ${#modes[@]} ))
        local mode=${modes[$index]}
        local stem=$OUT/raw/pmu/$family/${case_name}-${mode}-run$(printf '%02d' "$run")
        (cd "$ROOT"; sudo -n "$PERF" stat -x, -e cycles,instructions \
          -o "$stem.perf.csv" -- taskset -c "$CPU" env \
          LD_PRELOAD="$AGENT" BPFTIME_MAP_PRODUCTION_AB="$mode" \
          BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
          "$victim" "$ITER" "$run" "$WARMUP" "$case_name") \
          >"$stem.txt" 2>&1
        grep -q 'shm_open_type 1' "$stem.txt"
      done
    done
  done
  stop_loader
}

run_family array "$ARRAY_LOADER" "$ARRAY_VICTIM" ARRAY_SPECS
run_family hash "$HASH_LOADER" "$HASH_VICTIM" HASH_SPECS
python3 "$OUT/summarize-pmu.py"
