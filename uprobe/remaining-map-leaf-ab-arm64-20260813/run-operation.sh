#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <percpu_hash_update|percpu_array_update|percpu_array_lookup|array_update> <smoke|wall|pmu|profile>" >&2
  exit 2
fi

OP=$1
PHASE=$2
ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
DIAG=${DIAG:-/home/jetson/src/.worktrees/map-production-ab-arm64}
BUILD=${BUILD:-$DIAG/build-map-production-ab-arm64}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/remaining-map-leaf-ab-arm64-20260813}
CPU=${CPU:-5}
PERF=${PERF:-/usr/lib/linux-tools-6.8.0-134/perf}
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool

case "$OP" in
  percpu_hash_update)
    LOADER=$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure
    VICTIM=benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim
    CONTROL=update_control
    MODES=(base percpu_hash_update_raw_key_copy percpu_hash_update_raw_value_copy percpu_hash_update_cached_hash percpu_hash_update_fixed4_equal percpu_hash_update_cached_hash_fixed4_equal percpu_hash_update_no_copy percpu_hash_update_no_find)
    ;;
  percpu_array_update)
    LOADER=$ROOT/benchmark/uprobe/.output/helper-map-closure/helper-map-closure
    VICTIM=benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim
    CONTROL=update_control
    MODES=(base percpu_array_direct percpu_array_fixed_cpu percpu_array_update_address_only percpu_array_update_fixed_cpu_address_only percpu_array_update_no_address percpu_array_update_fixed_cpu_no_address percpu_array_update_no_body)
    ;;
  percpu_array_lookup)
    LOADER=$ROOT/benchmark/uprobe/.output/helper-map-closure/helper-map-closure
    VICTIM=benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim
    CONTROL=lookup_control
    MODES=(base percpu_array_direct percpu_array_fixed_cpu percpu_array_lookup_no_address percpu_array_lookup_fixed_cpu_no_address percpu_array_lookup_no_body)
    ;;
  array_update)
    LOADER=$ROOT/benchmark/uprobe/.output/helper-map-closure/helper-map-closure
    VICTIM=benchmark/uprobe/.output/helper-map-closure/helper-map-closure-victim
    CONTROL=update_control
    MODES=(base cache_control cached_handler direct_map array_update_address_only array_update_no_address array_update_no_body)
    ;;
  *) echo "unknown operation: $OP" >&2; exit 2 ;;
esac

case "$PHASE" in
  smoke) RUNS=1; ITER=10; WARMUP=1 ;;
  wall) RUNS=${RUNS:-5}; ITER=${ITER:-20000}; WARMUP=${WARMUP:-1000} ;;
  pmu) RUNS=${RUNS:-3}; ITER=${ITER:-20000}; WARMUP=${WARMUP:-1000} ;;
  profile) RUNS=1; ITER=${ITER:-10000}; WARMUP=${WARMUP:-1000} ;;
  *) echo "unknown phase: $PHASE" >&2; exit 2 ;;
esac

DEST=$OUT/$OP/raw/$PHASE
mkdir -p "$DEST"
sudo -n jetson_clocks
loader_pid=
cleanup() {
  set +e
  if [[ -n "$loader_pid" ]]; then
    sudo -n kill -TERM "$loader_pid" 2>/dev/null
    wait "$loader_pid" 2>/dev/null
  fi
  sudo -n "$TOOL" remove >/dev/null 2>&1
  sudo -n chown -R "$(id -u):$(id -g)" "$DEST" 2>/dev/null
}
trap cleanup EXIT INT TERM

sudo -n "$TOOL" remove >/dev/null 2>&1 || true
(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$LOADER") >"$DEST/loader.txt" 2>&1 &
loader_pid=$!
for _ in $(seq 1 200); do
  grep -q 'Successfully started' "$DEST/loader.txt" && break
  sudo -n kill -0 "$loader_pid" 2>/dev/null || exit 1
  sleep 0.1
done
grep -q 'Successfully started' "$DEST/loader.txt"
sleep 5

if [[ "$PHASE" == smoke ]]; then
  for mode in "${MODES[@]}"; do
    stats=()
    [[ "$OP" == percpu_hash_update ]] && stats=(BPFTIME_MAP_PRODUCTION_AB_STATS=1)
    stem=$DEST/$mode
    (cd "$ROOT"; sudo -n taskset -c "$CPU" env LD_PRELOAD="$AGENT" BPFTIME_MAP_PRODUCTION_AB="$mode" "${stats[@]}" BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info "$VICTIM" "$ITER" 0 "$WARMUP" "$OP") >"$stem.txt" 2>&1
    grep -q 'shm_open_type 1' "$stem.txt"
    grep -q "^$OP," "$stem.txt"
  done
elif [[ "$PHASE" == wall ]]; then
  for run in $(seq 1 "$RUNS"); do
    for offset in $(seq 0 $((${#MODES[@]} - 1))); do
      index=$(( (run - 1 + offset) % ${#MODES[@]} ))
      mode=${MODES[$index]}
      stem=$DEST/${mode}-run$(printf '%02d' "$run")
      (cd "$ROOT"; sudo -n /usr/bin/time -v -o "$stem.time.txt" taskset -c "$CPU" env LD_PRELOAD="$AGENT" BPFTIME_MAP_PRODUCTION_AB="$mode" BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info "$VICTIM" "$ITER" "$run" "$WARMUP") >"$stem.txt" 2>&1
      grep -q 'shm_open_type 1' "$stem.txt"
    done
  done
elif [[ "$PHASE" == pmu ]]; then
  CASES=("$CONTROL" "$OP")
  for run in $(seq 1 "$RUNS"); do
    for offset in $(seq 0 $((${#MODES[@]} - 1))); do
      index=$(( (run - 1 + offset) % ${#MODES[@]} ))
      mode=${MODES[$index]}
      for case_offset in 0 1; do
        case_index=$(( (run - 1 + case_offset) % 2 ))
        case_name=${CASES[$case_index]}
        stem=$DEST/${mode}-${case_name}-run$(printf '%02d' "$run")
        (cd "$ROOT"; sudo -n "$PERF" stat -x, -e cycles,instructions -o "$stem.perf.csv" -- taskset -c "$CPU" env LD_PRELOAD="$AGENT" BPFTIME_MAP_PRODUCTION_AB="$mode" BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info "$VICTIM" "$ITER" "$run" "$WARMUP" "$case_name") >"$stem.txt" 2>&1
        grep -q 'shm_open_type 1' "$stem.txt"
      done
    done
  done
else
  PROFILE_MODES=(base "${MODES[-1]}")
  for mode in "${PROFILE_MODES[@]}"; do
    stem=$DEST/$mode
    (cd "$ROOT"; sudo -n "$PERF" record -q -e cycles:u -F 999 -o "$stem.data" -- taskset -c "$CPU" env LD_PRELOAD="$AGENT" BPFTIME_MAP_PRODUCTION_AB="$mode" BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info "$VICTIM" "$ITER" 0 "$WARMUP" "$OP") >"$stem.txt" 2>&1
    sudo -n "$PERF" report -f --stdio --no-children -g none --percent-limit 0.2 -i "$stem.data" >"$stem.report.txt"
  done
fi

sudo -n kill -TERM "$loader_pid"
wait "$loader_pid" || true
loader_pid=
sudo -n "$TOOL" remove >/dev/null 2>&1
