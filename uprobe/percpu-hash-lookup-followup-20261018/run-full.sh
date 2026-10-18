#!/usr/bin/env bash
set -euo pipefail

ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
RESULTS=/home/jetson/src/results/uprobe/percpu-hash-lookup-followup-20261018
SNAPSHOTS=/tmp/bpftime-percpu-lookup-20261018.iyqaA3
WINNER=${1:?accepted short A/B phase required}
SNAPSHOT="$SNAPSHOTS/$WINNER"
OUT="$RESULTS/full10"
RUNNER="$SNAPSHOTS/full-runner/bpftime"

test "$(cat "$RESULTS/$WINNER/exit-status.txt")" = 0
test ! -e /dev/shm/bpftime_maps_shm
mkdir "$OUT"
mkdir -p "$SNAPSHOT/benchmark" "$RUNNER"
cp "$BUILD/benchmark/simple-benchmark-with-embed-ebpf-calling" \
	"$SNAPSHOT/benchmark/"
ln -s "$SNAPSHOT" "$RUNNER/build"
ln -s "$ROOT/benchmark" "$RUNNER/benchmark"
TOOL="$SNAPSHOT/tools/bpftimetool/bpftimetool"

finish()
{
	status=$?
	trap - EXIT
	sudo -n "$TOOL" remove >>"$OUT/cleanup.log" 2>&1 || true
	printf '%s\n' "$status" >"$OUT/exit-status.txt"
	date --iso-8601=seconds >"$OUT/finished-at.txt"
	exit "$status"
}
trap finish EXIT
cd "$ROOT"
date --iso-8601=seconds >"$OUT/started-at.txt"
printf '%s\n' "$WINNER" >"$OUT/selected-phase.txt"
git rev-parse HEAD >"$OUT/base-commit.txt"
git status --short >"$OUT/source-status.txt"
git diff -- runtime >"$OUT/source.diff"
cmp runtime/src/bpf_map/bpftime_hash_map.hpp \
	"$RESULTS/baseline/bpftime_hash_map.hpp"
cmp runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp \
	"$RESULTS/$WINNER/per_cpu_hash_map.cpp"
cmp runtime/src/bpf_map/userspace/per_cpu_hash_map.hpp \
	"$RESULTS/$WINNER/per_cpu_hash_map.hpp"
sha256sum "$SNAPSHOT/runtime/agent/libbpftime-agent.so" \
	"$SNAPSHOT/runtime/syscall-server/libbpftime-syscall-server.so" \
	>"$OUT/runtime-sha256.txt"
sudo -n /usr/sbin/nvpmodel -q >"$OUT/nvpmodel-before.txt" 2>&1
sudo -n jetson_clocks --show >"$OUT/jetson-clocks-before.txt" 2>&1

cd "$RUNNER"
sudo -n taskset -c 5 env PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
	/usr/bin/python3 -u benchmark/uprobe/benchmark.py --iter 10 \
	2>&1 | tee "$OUT/runner.log"
for artifact in benchmark-output.json results.md benchmark-logs.txt; do
	test "$ROOT/benchmark/uprobe/$artifact" -nt "$OUT/started-at.txt"
	cp "$ROOT/benchmark/uprobe/$artifact" "$OUT/"
done
python3 - "$OUT/benchmark-output.json" <<'PY'
import json
import math
import sys

with open(sys.argv[1]) as stream:
    result = json.load(stream)
for group, expected in (("kernel_uprobe", 17), ("userspace_uprobe", 17), ("embed", 1)):
    assert len(result[group]) == expected, group
    for name, values in result[group].items():
        assert len(values["raw_values"]) == 10, (group, name)
        assert all(math.isfinite(value) and value > 0 for value in values["raw_values"])
print("Full-suite result validation passed: 17 kernel, 17 userspace, 1 embedded case; ten rounds each.")
PY
sha256sum --check "$OUT/runtime-sha256.txt"
python3 "$RESULTS/summarize-full.py" >"$OUT/ten-operation-comparison.md"
