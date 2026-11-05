#!/bin/bash
# Builds the two experiment binaries in the scratch dir. Read-only on the repo:
# it only reads headers, sources and the already-built static libraries.
set -eu
WT=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=$WT/build-jetson-daily
OUT=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/review-20261105/loadtime
FM=$B/runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/flags.make
DEFS=$(grep '^CXX_DEFINES' "$FM" | sed 's/^CXX_DEFINES = //')
INCS=$(grep '^CXX_INCLUDES' "$FM" | sed 's/^CXX_INCLUDES = //')

# 1. analysis-only cases: compile the analysis source directly (no LTO objects)
g++-13 -std=gnu++20 -O1 -g -I"$WT/vm/vm-core/include" -I"$WT/vm/compat/include" \
	-I"$WT/runtime/src" "$OUT/reach_cases.cpp" "$WT/runtime/src/bpftime_prog_reachability.cpp" \
	-o "$OUT/reach_cases"
echo "built reach_cases"

# 2. differential fuzz: link against the unit-test's libraries
g++-13 -std=gnu++20 -O1 -g $DEFS $INCS -c "$OUT/diff_fuzz.cpp" -o "$OUT/diff_fuzz.o"
LINK=$(sed -E 's#CMakeFiles/bpftime_runtime_tests\.dir/[^ ]+\.o##g; s#-Wl,--wrap=[a-z_]+##g; s#\.\./\.\./third_party/Catch2/src/libCatch2Main\.a##; s#\.\./\.\./third_party/Catch2/src/libCatch2\.a##; s#-o bpftime_runtime_tests#-o '"$OUT"'/diff_fuzz '"$OUT"'/diff_fuzz.o#' \
	"$B/runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/link.txt")
cd "$B/runtime/unit-test"
eval "$LINK"
echo "built diff_fuzz"
