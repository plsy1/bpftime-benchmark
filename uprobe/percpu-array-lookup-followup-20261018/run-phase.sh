#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
SNAPSHOTS=/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb
RESULTS=/home/jetson/src/results/uprobe/percpu-array-lookup-followup-20261018
PHASE=${1:?phase required}
OUT="$RESULTS/$PHASE"
SNAPSHOT="$SNAPSHOTS/$PHASE"
mkdir "$OUT"
mkdir -p "$SNAPSHOT/runtime/agent" "$SNAPSHOT/runtime/syscall-server" \
	"$SNAPSHOT/tools/bpftimetool"
finish()
{
	status=$?
	printf '%s\n' "$status" >"$OUT/exit-status.txt"
	date --iso-8601=seconds >"$OUT/finished-at.txt"
	exit "$status"
}
trap finish EXIT
cd "$ROOT"
date --iso-8601=seconds >"$OUT/started-at.txt"
git rev-parse HEAD >"$OUT/base-commit.txt"
git diff -- runtime >"$OUT/source.diff"
cp runtime/src/bpf_map/userspace/per_cpu_array_map.cpp \
	runtime/src/bpf_map/userspace/per_cpu_array_map.hpp "$OUT/"
sha256sum --check "$RESULTS/baseline/preserved-hash-source-sha256.txt"
printf 'Checking per-CPU array correctness: %s\n' "$PHASE"
g++-13 -std=c++20 -O2 -g -fno-omit-frame-pointer \
	-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO -DSPDLOG_COMPILED_LIB \
	-Iruntime/include -Iruntime/src -Ithird_party/spdlog/include \
	-Ithird_party/Catch2/src \
	-I"$BUILD/third_party/Catch2/generated-includes" \
	runtime/unit-test/maps/test_per_cpu_array.cpp \
	runtime/src/bpf_map/userspace/per_cpu_array_map.cpp \
	runtime/src/platform_utils.cpp \
	"$BUILD/third_party/Catch2/src/libCatch2Main.a" \
	"$BUILD/third_party/Catch2/src/libCatch2.a" \
	"$BUILD/third_party/spdlog/libspdlog.a" -pthread -lrt \
	-o "$SNAPSHOT/percpu-array-tests" >"$OUT/test-build.log" 2>&1
"$SNAPSHOT/percpu-array-tests" | tee "$OUT/tests.log"
printf 'Building runtime: %s\n' "$PHASE"
cmake --build "$BUILD" --target bpftime-agent bpftime-syscall-server \
	bpftimetool -j2 >"$OUT/build.log" 2>&1
cp "$BUILD/runtime/agent/libbpftime-agent.so" "$SNAPSHOT/runtime/agent/"
cp "$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" "$SNAPSHOT/runtime/syscall-server/"
cp "$BUILD/tools/bpftimetool/bpftimetool" "$SNAPSHOT/tools/bpftimetool/"
cp "$BUILD/CMakeCache.txt" "$SNAPSHOT/"
printf 'Running paired five-round A/B: %s\n' "$PHASE"
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
	--baseline "$SNAPSHOTS/baseline" --candidate "$SNAPSHOT" \
	--output "$OUT/ab5" --suite array --runs 5 --iterations 50000 --warmup 3000
printf 'Phase completed: %s\n' "$PHASE"
