#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
OUT=/home/jetson/src/results/uprobe/percpu-hash-post-keyeq-profile-arm64-20261017
LOADER="$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure"
VICTIM=benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim
SERVER="$BUILD/runtime/syscall-server/libbpftime-syscall-server.so"
AGENT="$BUILD/runtime/agent/libbpftime-agent.so"
TOOL="$BUILD/tools/bpftimetool/bpftimetool"
CPU=5
ITERATIONS=100000
WARMUP=1000

loader_pid=
cleanup()
{
	status=$?
	set +e
	if [[ -n "$loader_pid" ]]; then
		sudo -n kill -TERM "$loader_pid" 2>/dev/null
		wait "$loader_pid" 2>/dev/null
	fi
	sudo -n "$TOOL" remove >>"$OUT/cleanup.log" 2>&1
	sudo -n chown -R "$(id -u):$(id -g)" "$OUT" 2>/dev/null
	printf '%s\n' "$status" >"$OUT/exit-status.txt"
	date --iso-8601=seconds >"$OUT/finished-at.txt"
	exit "$status"
}
trap cleanup EXIT INT TERM

date --iso-8601=seconds >"$OUT/started-at.txt"
git -C "$ROOT" rev-parse HEAD >"$OUT/source-commit.txt"
git -C "$ROOT" status --short >"$OUT/source-status.txt"
sudo -n jetson_clocks --show >"$OUT/jetson-clocks.txt" 2>&1
sudo -n "$TOOL" remove >"$OUT/cleanup.log" 2>&1 || true

(cd "$ROOT"; exec sudo -n taskset -c "$CPU" env \
	LD_PRELOAD="$SERVER" SPDLOG_LEVEL=info "$LOADER") \
	>"$OUT/loader.log" 2>&1 &
loader_pid=$!

for _ in $(seq 1 200); do
	grep -q 'Successfully started' "$OUT/loader.log" && break
	sudo -n kill -0 "$loader_pid" 2>/dev/null || exit 1
	sleep 0.1
done
grep -q 'Successfully started' "$OUT/loader.log"
sleep 2

(cd "$ROOT"; sudo -n nsys profile \
	--trace=none \
	--sample=process-tree \
	--run-as=root \
	--backtrace=dwarf \
	--samples-per-backtrace=1 \
	--sampling-period=1728000 \
	--force-overwrite=true \
	--output="$OUT/percpu-hash-lookup" \
	taskset -c "$CPU" env LD_PRELOAD="$AGENT" \
	BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
	"$VICTIM" "$ITERATIONS" 0 "$WARMUP" percpu_hash_lookup) \
	>"$OUT/victim.log" 2>"$OUT/nsys.log"

grep -q 'shm_open_type 1' "$OUT/nsys.log"
grep -q '^percpu_hash_lookup,' "$OUT/victim.log"
sudo -n nsys export --type sqlite --force-overwrite=true \
	--output="$OUT/percpu-hash-lookup.sqlite" \
	"$OUT/percpu-hash-lookup.nsys-rep" \
	>"$OUT/export.log" 2>&1
