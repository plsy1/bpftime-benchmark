#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
DIAG=${DIAG:-/home/jetson/src/.worktrees/map-production-ab-arm64}
BUILD=${BUILD:-$DIAG/build-map-production-ab-arm64}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/percpu-hash-lookup-leaf-ab-arm64-20260813}
CPU=${CPU:-5}
RUNS=${RUNS:-5}
ITER=${ITER:-20000}
WARMUP=${WARMUP:-1000}
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
LOADER=$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure
VICTIM=benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim
MODES=(base percpu_hash_lookup_raw_key_copy percpu_hash_lookup_cached_hash percpu_hash_lookup_fixed4_equal percpu_hash_lookup_cached_hash_fixed4_equal)

mkdir -p "$OUT/raw/wall"
sudo -n jetson_clocks
loader_pid=
cleanup() {
  set +e
  if [[ -n "$loader_pid" ]]; then
    sudo -n kill -TERM "$loader_pid" 2>/dev/null
    wait "$loader_pid" 2>/dev/null
  fi
  sudo -n "$TOOL" remove >/dev/null 2>&1
  sudo -n chown -R "$(id -u):$(id -g)" "$OUT/raw/wall" 2>/dev/null
}
trap cleanup EXIT INT TERM

sudo -n "$TOOL" remove >/dev/null 2>&1 || true
(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER" \
  SPDLOG_LEVEL=info "$LOADER") >"$OUT/raw/wall/loader.txt" 2>&1 &
loader_pid=$!
for _ in $(seq 1 200); do
  grep -q 'Successfully started' "$OUT/raw/wall/loader.txt" && break
  sudo -n kill -0 "$loader_pid" 2>/dev/null || exit 1
  sleep 0.1
done
grep -q 'Successfully started' "$OUT/raw/wall/loader.txt"
sleep 5

for run in $(seq 1 "$RUNS"); do
  for offset in $(seq 0 $((${#MODES[@]} - 1))); do
    index=$(( (run - 1 + offset) % ${#MODES[@]} ))
    mode=${MODES[$index]}
    stem=$OUT/raw/wall/${mode}-run$(printf '%02d' "$run")
    (cd "$ROOT"; sudo -n /usr/bin/time -v -o "$stem.time.txt" \
      taskset -c "$CPU" env LD_PRELOAD="$AGENT" \
      BPFTIME_MAP_PRODUCTION_AB="$mode" BPFTIME_LOG_OUTPUT=console \
      SPDLOG_LEVEL=info "$VICTIM" "$ITER" "$run" "$WARMUP") \
      >"$stem.txt" 2>&1
    grep -q 'shm_open_type 1' "$stem.txt"
  done
done

sudo -n kill -TERM "$loader_pid"
wait "$loader_pid" || true
loader_pid=
sudo -n "$TOOL" remove >/dev/null 2>&1
python3 "$OUT/summarize.py"
