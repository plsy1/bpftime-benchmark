#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
RESULTS=/home/jetson/src/results/uprobe/ordinary-array-lookup-cold-20261019
SNAPSHOTS=/tmp/bpftime-array-lookup-cold-20261019.8N6hqp
OLD=/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb/inline-leaf
PHASE=cold-errors
OUT="$RESULTS/$PHASE"
SNAPSHOT="$SNAPSHOTS/$PHASE"
finish()
{
    status=$?
    printf '%s\n' "$status" >"$RESULTS/pipeline-exit-status.txt"
    date --iso-8601=seconds >"$RESULTS/pipeline-finished-at.txt"
    exit "$status"
}
trap finish EXIT
cd "$ROOT"
test "$(git rev-parse HEAD)" = eb09b972d59ca7845e3e1251a2ad96845b93feaf
test ! -e /dev/shm/bpftime_maps_shm
mkdir "$OUT"
mkdir -p "$SNAPSHOT/runtime/agent" "$SNAPSHOT/runtime/syscall-server" \
    "$SNAPSHOT/tools/bpftimetool" "$SNAPSHOT/benchmark" "$OUT/source"
cp -a "$OLD" "$SNAPSHOTS/baseline"
date --iso-8601=seconds >"$RESULTS/pipeline-started-at.txt"
git rev-parse HEAD >"$OUT/base-commit.txt"
git diff -- runtime >"$OUT/source.diff"
for file in runtime/src/bpf_helper.cpp \
    runtime/src/bpf_map/userspace/array_map.cpp \
    runtime/src/bpf_map/userspace/array_map.hpp \
    runtime/unit-test/maps/test_per_cpu_array.cpp; do
    cp --parents "$file" "$OUT/source/"
done
sha256sum runtime/src/bpf_map/bpftime_hash_map.hpp \
    runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp \
    runtime/src/bpf_map/userspace/per_cpu_array_map.cpp \
    runtime/src/bpf_map/userspace/per_cpu_array_map.hpp \
    >"$RESULTS/preserved-source-sha256.txt"
printf 'Building cold-path candidate and current runtime tests.\n'
cmake --build "$BUILD" --target bpftime-agent bpftime-syscall-server \
    bpftimetool bpftime_runtime_tests -j2 >"$OUT/build.log" 2>&1
printf 'Running array/hash correctness checks.\n'
"$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    2>&1 | tee "$OUT/tests.log"
cp "$BUILD/runtime/agent/libbpftime-agent.so" "$SNAPSHOT/runtime/agent/"
cp "$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" "$SNAPSHOT/runtime/syscall-server/"
cp "$BUILD/tools/bpftimetool/bpftimetool" "$SNAPSHOT/tools/bpftimetool/"
cp "$BUILD/benchmark/simple-benchmark-with-embed-ebpf-calling" "$SNAPSHOT/benchmark/"
cp "$BUILD/CMakeCache.txt" "$SNAPSHOT/"
sha256sum --check "$RESULTS/preserved-source-sha256.txt"
printf 'Checking production assembly before measuring.\n'
python3 "$RESULTS/check-assembly.py"
printf 'Starting six-round balanced array A/B.\n'
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    --baseline "$SNAPSHOTS/baseline" --candidate "$SNAPSHOT" \
    --output "$OUT/array-ab6" --suite array --runs 6 --iterations 50000 --warmup 3000
python3 - "$OUT/array-ab6/summary.json" <<'PY'
import json,sys
rows=json.load(open(sys.argv[1]))
target=next(r for r in rows if r['case']=='array_lookup')
wins=sum(c<b for b,c in zip(target['baseline']['raw_values'],target['candidate']['raw_values']))
assert target['reduction_percent']>=2 and wins>=5, 'Insufficient stable lookup benefit; stop for review'
for row in rows:
    ratio=row['candidate']['median']/row['baseline']['median']
    print(row['case'], 'candidate/baseline', ratio, flush=True)
    assert ratio<=1.03, 'Array regression needs review: '+row['case']
print('Array screen passed:',target['reduction_percent'],'percent lower median;',wins,'of 6 wins',flush=True)
PY
printf 'Starting six-round hash regression screen.\n'
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    --baseline "$SNAPSHOTS/baseline" --candidate "$SNAPSHOT" \
    --output "$OUT/hash-ab6" --suite hash --runs 6 --iterations 50000 --warmup 3000
python3 - "$OUT/hash-ab6/summary.json" <<'PY'
import json,sys
for row in json.load(open(sys.argv[1])):
    ratio=row['candidate']['median']/row['baseline']['median']
    print(row['case'], 'candidate/baseline', ratio, flush=True)
    if row['case']!='hash_update':
        assert ratio<=1.03, 'Hash regression needs review: '+row['case']
    else:
        print('Ordinary hash update has known bimodality: inspect raw values in full-suite review.',flush=True)
print('Hash screen passed. Starting full ten-round validation.',flush=True)
PY
bash "$RESULTS/run-full.sh"
