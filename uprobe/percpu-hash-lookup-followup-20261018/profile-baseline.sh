#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD=/tmp/bpftime-percpu-lookup-20261018.iyqaA3/baseline
OUT=/home/jetson/src/results/uprobe/percpu-hash-lookup-followup-20261018/profile
LOADER="$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure"
VICTIM="$ROOT/benchmark/uprobe/.output/helper-hash-closure/helper-hash-closure-victim"
mkdir "$OUT"
test ! -e /dev/shm/bpftime_maps_shm
loader_pid=
cleanup()
{
	status=$?
	trap - EXIT
	set +e
	if [[ -n "$loader_pid" ]]; then
		sudo -n kill -TERM "$loader_pid" 2>/dev/null
		wait "$loader_pid" 2>/dev/null
	fi
	sudo -n "$BUILD/tools/bpftimetool/bpftimetool" remove >>"$OUT/cleanup.log" 2>&1
	printf '%s\n' "$status" >"$OUT/exit-status.txt"
	date --iso-8601=seconds >"$OUT/finished-at.txt"
	exit "$status"
}
trap cleanup EXIT
cd "$ROOT"
date --iso-8601=seconds >"$OUT/started-at.txt"
git rev-parse HEAD >"$OUT/source-commit.txt"
git diff -- runtime >"$OUT/source.diff"
sha256sum "$BUILD/runtime/agent/libbpftime-agent.so" \
	"$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" >"$OUT/runtime-sha256.txt"
sudo -n jetson_clocks --show >"$OUT/jetson-clocks.txt" 2>&1
(exec sudo -n taskset -c 5 env \
	LD_PRELOAD="$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" \
	SPDLOG_LEVEL=info "$LOADER") >"$OUT/loader.log" 2>&1 &
loader_pid=$!
for attempt in $(seq 1 200); do
	if rg -q 'Successfully started' "$OUT/loader.log"; then break; fi
	sudo -n kill -0 "$loader_pid" 2>/dev/null
	sleep 0.1
done
rg -q 'Successfully started' "$OUT/loader.log"
sleep 1
sudo -n nsys profile --trace=none --sample=process-tree --run-as=root \
	--backtrace=dwarf --samples-per-backtrace=1 --sampling-period=1728000 \
	--output="$OUT/percpu-hash-lookup" \
	taskset -c 5 env LD_PRELOAD="$BUILD/runtime/agent/libbpftime-agent.so" \
	BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
	"$VICTIM" 200000 0 3000 percpu_hash_lookup \
	>"$OUT/victim.log" 2>"$OUT/nsys.log"
rg -q 'shm_open_type 1' "$OUT/nsys.log"
rg -q '^percpu_hash_lookup,' "$OUT/victim.log"
sudo -n nsys export --type sqlite --output="$OUT/percpu-hash-lookup.sqlite" \
	"$OUT/percpu-hash-lookup.nsys-rep" >"$OUT/export.log" 2>&1
printf 'Baseline profile completed.\n'
