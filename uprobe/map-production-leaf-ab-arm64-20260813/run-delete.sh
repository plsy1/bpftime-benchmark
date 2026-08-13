#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
DIAG=${DIAG:-/home/jetson/src/.worktrees/map-production-ab-arm64}
BUILD=${BUILD:-$DIAG/build-map-production-ab-arm64}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/map-production-leaf-ab-arm64-20260813}
CPU=${CPU:-5}
RUNS=${RUNS:-5}
ITER=${ITER:-1000}

AGENT=$BUILD/runtime/agent/libbpftime-agent.so
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
LOADER=benchmark/uprobe/uprobe
VICTIM=benchmark/test
MODES=(base percpu_hash_delete_defer_reclaim)

mkdir -p "$OUT/raw/delete"
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
  sudo -n chown -R "$(id -u):$(id -g)" "$OUT/raw/delete" 2>/dev/null
}
trap cleanup EXIT INT TERM

sudo -n "$TOOL" remove >/dev/null 2>&1 || true
(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER" \
  SPDLOG_LEVEL=info "$LOADER") >"$OUT/raw/delete/loader.txt" 2>&1 &
loader_pid=$!
for _ in $(seq 1 200); do
  grep -q 'Successfully started' "$OUT/raw/delete/loader.txt" && break
  sudo -n kill -0 "$loader_pid" 2>/dev/null || { tail -100 "$OUT/raw/delete/loader.txt"; exit 1; }
  sleep 0.1
done
grep -q 'Starting syscall server' "$OUT/raw/delete/loader.txt"
grep -q 'Successfully started' "$OUT/raw/delete/loader.txt"
sleep 5

for run in $(seq 1 "$RUNS"); do
  for offset in 0 1; do
    index=$(( (run - 1 + offset) % 2 ))
    mode=${MODES[$index]}
    stem=$OUT/raw/delete/${mode}-run$(printf '%02d' "$run")
    (cd "$ROOT"; sudo -n /usr/bin/time -v -o "$stem.time.txt" \
      taskset -c "$CPU" env LD_PRELOAD="$AGENT" \
      BPFTIME_MAP_PRODUCTION_AB="$mode" BPFTIME_LOG_OUTPUT=console \
      SPDLOG_LEVEL=info "$VICTIM" 1 "$ITER") >"$stem.txt" 2>&1
    grep -q 'shm_open_type 1' "$stem.txt"
    grep -q 'Benchmarking __bench_per_cpu_hash_map_delete' "$stem.txt"
  done
done

sudo -n kill -TERM "$loader_pid"
wait "$loader_pid" || true
loader_pid=
sudo -n "$TOOL" remove >/dev/null 2>&1
python3 "$OUT/summarize-delete.py"
