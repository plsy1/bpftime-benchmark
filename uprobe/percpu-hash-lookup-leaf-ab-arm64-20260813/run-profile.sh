#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/jetson/src/bpftime-offical-no-btf}
BUILD=${BUILD:-/home/jetson/src/.worktrees/map-production-ab-arm64/build-map-production-ab-arm64}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/percpu-hash-lookup-leaf-ab-arm64-20260813}
CPU=${CPU:-5}
PERF=${PERF:-/usr/lib/linux-tools-6.8.0-134/perf}
SERVER=$BUILD/runtime/syscall-server/libbpftime-syscall-server.so
AGENT=$BUILD/runtime/agent/libbpftime-agent.so
TOOL=$BUILD/tools/bpftimetool/bpftimetool
LOADER=$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure
VICTIM=benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim
MODES=(base percpu_hash_lookup_cached_hash_fixed4_equal)

mkdir -p "$OUT/raw/profile"
loader_pid=
cleanup() {
  set +e
  if [[ -n "$loader_pid" ]]; then
    sudo -n kill -TERM "$loader_pid" 2>/dev/null
    wait "$loader_pid" 2>/dev/null
  fi
  sudo -n "$TOOL" remove >/dev/null 2>&1
  sudo -n chown -R "$(id -u):$(id -g)" "$OUT/raw/profile" 2>/dev/null
}
trap cleanup EXIT INT TERM

sudo -n "$TOOL" remove >/dev/null 2>&1 || true
(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$LOADER") \
  >"$OUT/raw/profile/loader.txt" 2>&1 &
loader_pid=$!
for _ in $(seq 1 200); do
  grep -q 'Successfully started' "$OUT/raw/profile/loader.txt" && break
  sudo -n kill -0 "$loader_pid" 2>/dev/null || exit 1
  sleep 0.1
done
grep -q 'Successfully started' "$OUT/raw/profile/loader.txt"
sleep 5

for mode in "${MODES[@]}"; do
  data=$OUT/raw/profile/$mode.data
  (cd "$ROOT"; sudo -n "$PERF" record -q -e cycles:u -F 999 \
    -o "$data" -- taskset -c "$CPU" env \
    LD_PRELOAD="$AGENT" BPFTIME_MAP_PRODUCTION_AB="$mode" \
    BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
    "$VICTIM" 10000 0 1000 percpu_hash_lookup) \
    >"$OUT/raw/profile/$mode.stdout.txt" 2>"$OUT/raw/profile/$mode.stderr.txt"
  sudo -n "$PERF" report -f --stdio --no-children -g none --percent-limit 0.2 \
    -i "$data" >"$OUT/raw/profile/$mode.report.txt"
done

sudo -n kill -TERM "$loader_pid"
wait "$loader_pid" || true
loader_pid=
sudo -n "$TOOL" remove >/dev/null 2>&1
