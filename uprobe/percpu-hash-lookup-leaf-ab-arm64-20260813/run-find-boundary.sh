#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
BUILD=${BUILD:-/home/jetson/src/.worktrees/map-production-ab-arm64/build-map-production-ab-arm64}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/percpu-hash-lookup-leaf-ab-arm64-20260813}
CPU=${CPU:-5}
ITER=${ITER:-20000}
WARMUP=${WARMUP:-1000}
PERF=${PERF:-/usr/lib/linux-tools-6.8.0-134/perf}
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
LOADER=$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure
VICTIM=benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim
MODES=(base percpu_hash_lookup_no_find)
CASES=(lookup_control percpu_hash_lookup)

mkdir -p "$OUT/raw/find-boundary/wall" "$OUT/raw/find-boundary/pmu"
sudo -n jetson_clocks
loader_pid=
cleanup() {
  set +e
  if [[ -n "$loader_pid" ]]; then
    sudo -n kill -TERM "$loader_pid" 2>/dev/null
    wait "$loader_pid" 2>/dev/null
  fi
  sudo -n "$TOOL" remove >/dev/null 2>&1
  sudo -n chown -R "$(id -u):$(id -g)" "$OUT/raw/find-boundary" 2>/dev/null
}
trap cleanup EXIT INT TERM

sudo -n "$TOOL" remove >/dev/null 2>&1 || true
(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$LOADER") \
  >"$OUT/raw/find-boundary/loader.txt" 2>&1 &
loader_pid=$!
for _ in $(seq 1 200); do
  grep -q 'Successfully started' "$OUT/raw/find-boundary/loader.txt" && break
  sudo -n kill -0 "$loader_pid" 2>/dev/null || exit 1
  sleep 0.1
done
grep -q 'Successfully started' "$OUT/raw/find-boundary/loader.txt"
sleep 5

for run in $(seq 1 5); do
  for offset in 0 1; do
    index=$(( (run - 1 + offset) % 2 ))
    mode=${MODES[$index]}
    stem=$OUT/raw/find-boundary/wall/${mode}-run$(printf '%02d' "$run")
    (cd "$ROOT"; sudo -n /usr/bin/time -v -o "$stem.time.txt" taskset -c "$CPU" \
      env LD_PRELOAD="$AGENT" BPFTIME_MAP_PRODUCTION_AB="$mode" \
      BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
      "$VICTIM" "$ITER" "$run" "$WARMUP") >"$stem.txt" 2>&1
    grep -q 'shm_open_type 1' "$stem.txt"
  done
done

for run in $(seq 1 3); do
  for offset in 0 1; do
    index=$(( (run - 1 + offset) % 2 ))
    mode=${MODES[$index]}
    for case_offset in 0 1; do
      case_index=$(( (run - 1 + case_offset) % 2 ))
      case_name=${CASES[$case_index]}
      stem=$OUT/raw/find-boundary/pmu/${mode}-${case_name}-run$(printf '%02d' "$run")
      (cd "$ROOT"; sudo -n "$PERF" stat -x, -e cycles,instructions \
        -o "$stem.perf.csv" -- taskset -c "$CPU" env LD_PRELOAD="$AGENT" \
        BPFTIME_MAP_PRODUCTION_AB="$mode" BPFTIME_LOG_OUTPUT=console \
        SPDLOG_LEVEL=info "$VICTIM" "$ITER" "$run" "$WARMUP" "$case_name") \
        >"$stem.txt" 2>&1
      grep -q 'shm_open_type 1' "$stem.txt"
    done
  done
done

sudo -n kill -TERM "$loader_pid"
wait "$loader_pid" || true
loader_pid=
sudo -n "$TOOL" remove >/dev/null 2>&1
python3 "$OUT/summarize-boundary.py"
