#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
RESULTS=/home/jetson/src/results/uprobe/percpu-array-address-dependency-20261019
SNAPSHOTS=/tmp/bpftime-percpu-array-address-20261019.S1mHZP
OLD=/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors
OUT="$RESULTS/address-first"
SNAPSHOT="$SNAPSHOTS/address-first"
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
mkdir "$OUT"
mkdir -p "$SNAPSHOT/runtime/agent" "$SNAPSHOT/runtime/syscall-server" \
    "$SNAPSHOT/tools/bpftimetool" "$SNAPSHOT/benchmark" "$OUT/source"
cp -a "$OLD" "$SNAPSHOTS/baseline"
date --iso-8601=seconds >"$RESULTS/pipeline-started-at.txt"
git rev-parse HEAD >"$OUT/base-commit.txt"
git diff -- runtime >"$OUT/source.diff"
for file in runtime/src/bpf_map/userspace/per_cpu_array_map.hpp \
    runtime/unit-test/maps/test_per_cpu_array.cpp; do
    cp --parents "$file" "$OUT/source/"
done
sha256sum runtime/src/bpf_helper.cpp \
    runtime/src/bpf_map/bpftime_hash_map.hpp \
    runtime/src/bpf_map/userspace/per_cpu_hash_map.cpp \
    runtime/src/bpf_map/userspace/per_cpu_hash_map.hpp \
    runtime/src/bpf_map/userspace/array_map.cpp \
    runtime/src/bpf_map/userspace/array_map.hpp \
    runtime/src/bpf_map/userspace/per_cpu_array_map.cpp \
    >"$RESULTS/preserved-source-sha256.txt"
printf 'Building isolated CPU query-count correctness test.\n'
g++-13 -std=c++20 -O2 -g -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO \
    -DSPDLOG_COMPILED_LIB -Iruntime/include -Iruntime/src \
    -Ithird_party/spdlog/include -Ithird_party/Catch2/src \
    -I"$BUILD/third_party/Catch2/generated-includes" \
    "$RESULTS/test-query-count.cpp" runtime/src/bpf_map/userspace/per_cpu_array_map.cpp \
    "$BUILD/third_party/Catch2/src/libCatch2Main.a" \
    "$BUILD/third_party/Catch2/src/libCatch2.a" \
    "$BUILD/third_party/spdlog/libspdlog.a" -pthread -lrt \
    -o "$SNAPSHOTS/query-count-tests" >"$OUT/query-count-build.log" 2>&1
"$SNAPSHOTS/query-count-tests" 2>&1 | tee "$OUT/query-count-tests.log"
printf 'Building address-first candidate and current runtime tests.\n'
cmake --build "$BUILD" --target bpftime-agent bpftime-syscall-server \
    bpftimetool bpftime_runtime_tests -j2 >"$OUT/build.log" 2>&1
printf 'Running production array/hash correctness checks.\n'
"$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    2>&1 | tee "$OUT/tests.log"
cp "$BUILD/runtime/agent/libbpftime-agent.so" "$SNAPSHOT/runtime/agent/"
cp "$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" "$SNAPSHOT/runtime/syscall-server/"
cp "$BUILD/tools/bpftimetool/bpftimetool" "$SNAPSHOT/tools/bpftimetool/"
cp "$BUILD/benchmark/simple-benchmark-with-embed-ebpf-calling" "$SNAPSHOT/benchmark/"
cp "$BUILD/CMakeCache.txt" "$SNAPSHOT/"
sha256sum --check "$RESULTS/preserved-source-sha256.txt"
printf 'Inspecting generated array helper assembly.\n'
python3 "$RESULTS/check-assembly.py"
printf 'Starting balanced six-round array A/B.\n'
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    --baseline "$SNAPSHOTS/baseline" --candidate "$SNAPSHOT" \
    --output "$OUT/array-ab6" --suite array --runs 6 --iterations 50000 --warmup 3000
python3 - "$OUT/array-ab6/summary.json" <<'PY'
import json,sys
rows=json.load(open(sys.argv[1]))
target=next(r for r in rows if r['case']=='percpu_array_lookup')
wins=sum(c<b for b,c in zip(target['baseline']['raw_values'],target['candidate']['raw_values']))
for row in rows:
    ratio=row['candidate']['median']/row['baseline']['median']
    print(row['case'],'candidate/baseline',ratio,flush=True)
    assert ratio<=1.03,'Array regression requires review: '+row['case']
assert target['reduction_percent']>=2 and wins>=5,'Insufficient stable per-CPU lookup benefit; stop before full run'
print('Array gate passed:',target['reduction_percent'],'percent improvement;',wins,'of 6 wins',flush=True)
PY
printf 'Starting balanced six-round hash regression screen.\n'
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
    --baseline "$SNAPSHOTS/baseline" --candidate "$SNAPSHOT" \
    --output "$OUT/hash-ab6" --suite hash --runs 6 --iterations 50000 --warmup 3000
python3 - "$OUT/hash-ab6/summary.json" <<'PY'
import json,sys
for row in json.load(open(sys.argv[1])):
    ratio=row['candidate']['median']/row['baseline']['median']
    print(row['case'],'candidate/baseline',ratio,flush=True)
    assert ratio<=1.03,'Hash regression requires review: '+row['case']
print('All short-test gates passed. Starting full ten-round validation.',flush=True)
PY
bash "$RESULTS/run-full.sh"
