#!/usr/bin/env bash
# Replay the daily patch stack plus the two array JIT commits onto the latest
# official snapshot, then build and test it exactly like the daily candidate
# step (tracker.py build: GCC 13, LLVM/clang 15, RelWithDebInfo, LTO).
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/daily-jit-stack-validation-20261021
AUTOMATION=/home/jetson/src/bpftime-jetson-daily
OFFICIAL_SHA=177a1dc465ff6e4122e3bda9b280018c9a324b80
WT=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/jit-stack-validation
OFFICIAL_TESTS=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/official/build-jetson-daily/runtime/unit-test/bpftime_runtime_tests
NEW_COMMITS=(9ffbffba8325b92eaa0771345c4054ee82121ca6 c0939bdf8e3ece38f56d964cac3d333468b8bf23)
EXTRA_FILTER='[array-jit-lifetime],[array-jit-poc],[array-offset-branch]'
HOST_FILTER='[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops'

trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/exit-status.txt"; date --iso-8601=seconds >"$OUT/finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"

BASE_FILTER=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["build"]["test_filter"])' "$AUTOMATION/tools/jetson-upstream/config.json")
FILTER="$BASE_FILTER,$EXTRA_FILTER"
printf '%s\n' "$FILTER" >"$OUT/test-filter.txt"
python3 -I -c 'import json,sys; [print(c["sha"]) for c in json.load(open(sys.argv[1]))["candidate"]["commits"]]' \
    "$AUTOMATION/tools/jetson-upstream/config.json" >"$OUT/stack.txt"
printf '%s\n' "${NEW_COMMITS[@]}" >>"$OUT/stack.txt"

if [[ -e "$WT" ]]; then
    echo "validation worktree already exists: $WT" >&2
    exit 1
fi
git -C "$AUTOMATION" worktree add --detach "$WT" "$OFFICIAL_SHA" >"$OUT/migrate.log" 2>&1
while read -r sha; do
    echo "== cherry-pick $sha" >>"$OUT/migrate.log"
    git -C "$WT" cherry-pick -x "$sha" >>"$OUT/migrate.log" 2>&1
done <"$OUT/stack.txt"
git -C "$WT" rev-parse HEAD >"$OUT/candidate-sha.txt"
# Tree identity versus the validated 8da867d541f8 branch, excluding the single
# upstream commit between the two official snapshots.
git -C "$WT" diff --stat c0939bdf8e3ece38f56d964cac3d333468b8bf23 HEAD >"$OUT/tree-diff-vs-c0939bd.txt"

python3 "$AUTOMATION/tools/jetson-upstream/tracker.py" build "$WT" >"$OUT/build.log" 2>&1
grep -E '^(LLVM_DIR|BPFTIME_LLVM_JIT|CMAKE_BUILD_TYPE|BPFTIME_ENABLE_LTO|BPFTIME_BPF_CLANG)' \
    "$WT/build-jetson-daily/CMakeCache.txt" >"$OUT/cmake-cache.txt"
grep -E 'LLVM_PACKAGE_VERSION|LLVM_VERSION' "$WT/build-jetson-daily/CMakeCache.txt" >>"$OUT/cmake-cache.txt" || true

TESTS="$WT/build-jetson-daily/runtime/unit-test/bpftime_runtime_tests"
(cd "$WT" && env -u BPFTIME_ARRAY_JIT_INLINE timeout 600 "$TESTS" "$FILTER") >"$OUT/candidate-tests.log" 2>&1
(cd "$WT" && env -u BPFTIME_ARRAY_JIT_INLINE timeout 600 "$TESTS" "$HOST_FILTER") >"$OUT/candidate-host-contract.log" 2>&1
# The daily job applies the same filter to the official build first; tags that
# exist only in the candidate must not make that gate fail.
(cd /home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/official && timeout 600 "$OFFICIAL_TESTS" "$FILTER") >"$OUT/official-tests.log" 2>&1
(cd /home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/official && timeout 600 "$OFFICIAL_TESTS" "$HOST_FILTER") >"$OUT/official-host-contract.log" 2>&1
