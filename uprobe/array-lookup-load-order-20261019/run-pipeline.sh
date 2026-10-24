#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
RESULTS=/home/jetson/src/results/uprobe/array-lookup-load-order-20261019
BASE=/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors
SNAPSHOT=/tmp/bpftime-array-load-order-20261019.Qbnxir/load-order
finish()
{
    status=$?
    printf '%s\n' "$status" >"$RESULTS/pipeline-exit-status.txt"
    date --iso-8601=seconds >"$RESULTS/pipeline-finished-at.txt"
    exit "$status"
}
trap finish EXIT
cd "$ROOT"
test "$(git rev-parse HEAD)" = fefe2f6edf276e1b2957711203b5022dd1be8d7c
test ! -e /dev/shm/bpftime_maps_shm
date --iso-8601=seconds >"$RESULTS/pipeline-started-at.txt"
git diff -- runtime >"$RESULTS/source.diff"
git status --short >"$RESULTS/source-status.txt"
sha256sum benchmark/uprobe/Makefile \
    benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    benchmark/uprobe/diagnostics/hash_workload_sweep.bpf.c \
    benchmark/uprobe/diagnostics/hash_candidate_screen.py \
    benchmark/uprobe/diagnostics/hash_distribution_validation.py \
    >"$RESULTS/preserved-user-files.sha256"
mkdir -p "$RESULTS/source" "$SNAPSHOT/runtime/agent" \
    "$SNAPSHOT/runtime/syscall-server" "$SNAPSHOT/tools/bpftimetool" "$SNAPSHOT/benchmark"
for file in runtime/src/bpf_map/userspace/array_map.cpp \
    runtime/unit-test/maps/test_per_cpu_array.cpp; do
    cp --parents "$file" "$RESULTS/source/"
done
printf 'Building production agent for assembly inspection.\n'
cmake --build "$BUILD" --target bpftime-agent -j2 >"$RESULTS/build-agent.log" 2>&1
python3 "$RESULTS/check-assembly.py"
printf 'Building other artifacts and correctness tests.\n'
cmake --build "$BUILD" --target bpftime-syscall-server bpftimetool \
    bpftime_runtime_tests -j2 >"$RESULTS/build-tests.log" 2>&1
"$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    2>&1 | tee "$RESULTS/tests.log"
for artifact in runtime/agent/libbpftime-agent.so \
    runtime/syscall-server/libbpftime-syscall-server.so tools/bpftimetool/bpftimetool \
    benchmark/simple-benchmark-with-embed-ebpf-calling CMakeCache.txt; do
    cp "$BUILD/$artifact" "$SNAPSHOT/$artifact"
done
sha256sum --check "$RESULTS/preserved-user-files.sha256"
printf 'Running six-round balanced array A/B.\n'
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    --baseline "$BASE" --candidate "$SNAPSHOT" --output "$RESULTS/array-ab6" \
    --suite array --runs 6 --iterations 50000 --warmup 3000
python3 - "$RESULTS/array-ab6/summary.json" <<'PY'
import json,sys
rows=json.load(open(sys.argv[1]))
target=next(r for r in rows if r['case']=='array_lookup')
wins=sum(c<b for b,c in zip(target['baseline']['raw_values'],target['candidate']['raw_values']))
assert target['reduction_percent']>=2 and wins>=5, 'Insufficient stable target benefit; stop for review'
for row in rows:
    assert row['candidate']['median']/row['baseline']['median']<=1.03, 'Array regression: '+row['case']
print('Array screen passed; run hash regression screen next.',flush=True)
PY
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    --baseline "$BASE" --candidate "$SNAPSHOT" --output "$RESULTS/hash-ab6" \
    --suite hash --runs 6 --iterations 50000 --warmup 3000
printf 'Short screening complete; review all operations before full run.\n'
