#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
RESULTS=/home/jetson/src/results/uprobe/percpu-array-lookup-cold-20261019
SNAPSHOTS=/tmp/bpftime-percpu-array-cold-20261019.YHrmOV
SNAPSHOT="$SNAPSHOTS/cold-errors"
OUT="$RESULTS/full10"
RUNNER="$SNAPSHOTS/full-runner/bpftime"
TOOL="$SNAPSHOT/tools/bpftimetool/bpftimetool"
python3 - "$RESULTS/cold-errors/hash-ab6/summary.json" <<'PY'
import json,sys
for row in json.load(open(sys.argv[1])):
    ratio=row['candidate']['median']/row['baseline']['median']
    # Even the known bimodal ordinary-update case requires manual review
    # when its median increases by >3%; do not silently waive this check.
    assert ratio<=1.03, 'Full run held for regression review: '+row['case']
print('Final all-operation short-test regression gate passed.',flush=True)
PY
test ! -e /dev/shm/bpftime_maps_shm
mkdir "$OUT"
mkdir -p "$RUNNER"
ln -s "$SNAPSHOT" "$RUNNER/build"
ln -s "$ROOT/benchmark" "$RUNNER/benchmark"
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
git rev-parse HEAD >"$OUT/base-commit.txt"
git diff -- runtime >"$OUT/source.diff"
git status --short >"$OUT/source-status.txt"
sha256sum --check "$RESULTS/preserved-source-sha256.txt"
for file in runtime/src/bpf_helper.cpp \
    runtime/src/bpf_map/userspace/per_cpu_array_map.cpp \
    runtime/src/bpf_map/userspace/per_cpu_array_map.hpp \
    runtime/unit-test/maps/test_per_cpu_array.cpp; do
    cmp "$file" "$RESULTS/cold-errors/source/$file"
done
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
import json,math,sys
result=json.load(open(sys.argv[1]))
for group,count in [('kernel_uprobe',17),('userspace_uprobe',17),('embed',1)]:
    assert len(result[group])==count
    for name,value in result[group].items():
        assert len(value['raw_values'])==10,(group,name)
        assert all(math.isfinite(x) and x>0 for x in value['raw_values'])
print('Full-suite sample validation passed.',flush=True)
PY
sha256sum --check "$OUT/runtime-sha256.txt"
python3 "$RESULTS/summarize-full.py" >"$OUT/ten-operation-comparison.md"
