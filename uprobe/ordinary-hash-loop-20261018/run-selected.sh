#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
BUILD="$ROOT/build-uprobe-handler-fast-access-20261016"
RESULTS=/home/jetson/src/results/uprobe/ordinary-hash-loop-20261018
SNAPSHOTS=/tmp/bpftime-hash-loop-20261018.C7fgIJ
cd "$ROOT"
cmp runtime/src/bpf_map/bpftime_hash_map.hpp "$RESULTS/key-hoist/bpftime_hash_map.hpp"

printf 'Checking selected candidate with the expanded regression tests.\n'
g++-13 -std=c++20 -O2 -g \
	-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO -DSPDLOG_COMPILED_LIB \
	-Iruntime/include -Iruntime/src -Ithird_party/spdlog/include \
	-Ithird_party/Catch2/src \
	-I"$BUILD/third_party/Catch2/generated-includes" \
	runtime/unit-test/maps/test_fix_hash_map.cpp \
	runtime/src/bpf_map/userspace/fix_hash_map.cpp \
	"$BUILD/third_party/Catch2/src/libCatch2Main.a" \
	"$BUILD/third_party/Catch2/src/libCatch2.a" \
	"$BUILD/third_party/spdlog/libspdlog.a" -pthread -lrt \
	-o "$SNAPSHOTS/key-hoist/hash-tests-expanded" \
	>"$RESULTS/selected-test-build.log" 2>&1
"$SNAPSHOTS/key-hoist/hash-tests-expanded" '[maps][hash]' \
	| tee "$RESULTS/selected-tests.log"

printf 'Restoring the working build to the selected source.\n'
cmake --build "$BUILD" --target bpftime-agent bpftime-syscall-server \
	bpftimetool -j2 >"$RESULTS/selected-build.log" 2>&1
cmp "$BUILD/runtime/agent/libbpftime-agent.so" \
	"$SNAPSHOTS/key-hoist/runtime/agent/libbpftime-agent.so"
cmp "$BUILD/runtime/syscall-server/libbpftime-syscall-server.so" \
	"$SNAPSHOTS/key-hoist/runtime/syscall-server/libbpftime-syscall-server.so"
printf 'Working build matches the measured candidate. Starting full ten rounds.\n'
exec bash "$RESULTS/run-full.sh" key-hoist
