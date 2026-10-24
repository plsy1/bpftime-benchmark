#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
RESULTS=/home/jetson/src/results/uprobe/array-offset-branch-20261019
SNAPSHOTS=/tmp/bpftime-array-offset-branch-20261019.z1JKDF
SNAPSHOT="$SNAPSHOTS/branch"
OUT="$RESULTS/full10"
RUNNER="$SNAPSHOTS/full-runner/bpftime"
TOOL="$SNAPSHOT/tools/bpftimetool/bpftimetool"
python3 - "$RESULTS/array-ab6/summary.json" "$RESULTS/hash-ab6/summary.json" <<'PY'
import json,sys
for path in sys.argv[1:]:
    for row in json.load(open(path)):
        ratio=row['candidate']['median']/row['baseline']['median']
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
sha256sum --check "$RESULTS/preserved-user-files.sha256"
git diff --exit-code -- runtime/src/bpf_map/userspace/array_map.cpp
for file in runtime/src/bpf_map/offset_ptr_access.hpp \
    runtime/src/handler/map_handler.hpp \
    runtime/unit-test/maps/test_per_cpu_array.cpp; do
    cmp "$file" "$RESULTS/source/$file"
done
# Validate that the full run uses precisely the agent/server sampled in A/B.
python3 - "$RESULTS/hash-ab6/environment.json" <<'PY'
import hashlib,json,sys
meta=json.load(open(sys.argv[1]))
for name,digest in meta['binary_sha256'].items():
    with open(name,'rb') as f:
        assert hashlib.file_digest(f,'sha256').hexdigest()==digest,name
print('Short-test frozen artifacts verified.',flush=True)
PY
printf 'Rebuilding the embedded executable for this exact candidate.\n'
cmake --build "$ROOT/build-uprobe-handler-fast-access-20261016" \
    --target simple-benchmark-with-embed-ebpf-calling -j2 >"$OUT/build-embed.log" 2>&1
cp "$ROOT/build-uprobe-handler-fast-access-20261016/benchmark/simple-benchmark-with-embed-ebpf-calling" \
    "$SNAPSHOT/benchmark/"
sha256sum "$SNAPSHOT/runtime/agent/libbpftime-agent.so" \
    "$SNAPSHOT/runtime/syscall-server/libbpftime-syscall-server.so" \
    "$SNAPSHOT/benchmark/simple-benchmark-with-embed-ebpf-calling" \
    >"$OUT/runtime-sha256.txt"
sudo -n /usr/sbin/nvpmodel -q >"$OUT/nvpmodel-before.txt" 2>&1
sudo -n jetson_clocks --show >"$OUT/jetson-clocks-before.txt" 2>&1
python3 - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur'))
PY
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
sudo -n jetson_clocks --show >"$OUT/jetson-clocks-after.txt" 2>&1
python3 "$RESULTS/summarize-full.py" >"$OUT/ten-operation-comparison.md"
