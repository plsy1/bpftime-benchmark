#!/usr/bin/env bash
# Round 4: replay the current daily config (42 commits) plus the five commits
# made since (f24dc9b diagnostic, baf0cc85, c3090b59, 1cb20807, f26cc335) onto
# official 177a1dc in a fresh worktree, build with tracker.py build (daily
# GCC 13 / LLVM 15 / LTO settings), and run the configured test_filter on both
# the candidate and the official build. config.json itself is not changed.
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/daily-jit-stack-validation-20261021/round4-hash-fixes
AUTOMATION=/home/jetson/src/bpftime-jetson-daily
CONFIG=$AUTOMATION/tools/jetson-upstream/config.json
OFFICIAL_SHA=177a1dc465ff6e4122e3bda9b280018c9a324b80
WT=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/jit-stack-validation-4
OFFICIAL_WT=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/official

trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/exit-status.txt"; date --iso-8601=seconds >"$OUT/finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"
cp "$CONFIG" "$OUT/config.json"
FILTER=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["build"]["test_filter"])' "$CONFIG")
printf '%s\n' "$FILTER" >"$OUT/test-filter.txt"
python3 -I -c 'import json,sys; [print(c["sha"]) for c in json.load(open(sys.argv[1]))["candidate"]["commits"]]' \
    "$CONFIG" >"$OUT/stack.txt"
cat >>"$OUT/stack.txt" <<'STACK'
f24dc9bf93f960df0b7cf25b232c41ae1fc84b5d
baf0cc85eba4fac6410a09902df411c27b30b7d7
c3090b598241f2b97dc09a38e96708db04b56330
1cb208076d743e422205e97f781da03530b09e50
f26cc335f79725a926ee0bf057dc5996e97c63d4
STACK

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
git -C "$WT" diff --stat f26cc335f79725a926ee0bf057dc5996e97c63d4 HEAD >"$OUT/tree-diff-vs-f26cc335.txt"

python3 "$AUTOMATION/tools/jetson-upstream/tracker.py" build "$WT" >"$OUT/build.log" 2>&1
grep -E '^(LLVM_DIR|BPFTIME_LLVM_JIT|CMAKE_BUILD_TYPE|BPFTIME_ENABLE_LTO|BPFTIME_BPF_CLANG)' \
    "$WT/build-jetson-daily/CMakeCache.txt" >"$OUT/cmake-cache.txt"

status=0
(cd "$WT" && timeout 600 build-jetson-daily/runtime/unit-test/bpftime_runtime_tests "$FILTER") \
    >"$OUT/candidate-tests.log" 2>&1 || status=$?
printf '%s\n' "$status" >"$OUT/candidate-tests-exit.txt"
ostatus=0
(cd "$OFFICIAL_WT" && timeout 600 build-jetson-daily/runtime/unit-test/bpftime_runtime_tests "$FILTER") \
    >"$OUT/official-tests.log" 2>&1 || ostatus=$?
printf '%s\n' "$ostatus" >"$OUT/official-tests-exit.txt"
[[ $status -eq 0 && $ostatus -eq 0 ]]
