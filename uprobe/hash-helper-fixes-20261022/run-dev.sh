#!/usr/bin/env bash
# Dev build of the uncommitted working tree (LLVM 16, baseline options),
# focused tests, frozen dev artifacts, then the hash gap runner on them.
set -euo pipefail
TAG=$1
OUT=/home/jetson/src/results/uprobe/hash-helper-fixes-20261022/$TAG
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
BUILD="$ROOT/build-jit"
mkdir "$OUT"
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/exit-status.txt"; date --iso-8601=seconds >"$OUT/finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"
git -C "$ROOT" rev-parse HEAD >"$OUT/head.txt"
git -C "$ROOT" diff HEAD >"$OUT/working-tree.diff"
cmake --build "$BUILD" --parallel 6 --target bpftime_runtime_tests bpftime-agent bpftime-syscall-server bpftimetool >"$OUT/build.log" 2>&1
FILTER=$(python3 -I -c 'import json,sys; print(json.load(open(sys.argv[1]))["build"]["test_filter"])' \
    /home/jetson/src/bpftime-jetson-daily/tools/jetson-upstream/config.json)
(cd "$ROOT" && timeout 600 "$BUILD/runtime/unit-test/bpftime_runtime_tests" "$FILTER") >"$OUT/tests.log" 2>&1
A="$OUT/artifacts"
for item in runtime/agent/libbpftime-agent.so runtime/syscall-server/libbpftime-syscall-server.so tools/bpftimetool/bpftimetool; do
    mkdir -p "$A/$(dirname "$item")"; cp "$BUILD/$item" "$A/$item"
done
test ! -e /dev/shm/bpftime_maps_shm
GAP_BUILD="$A" python3 -u "${GAP_RUNNER:-/home/jetson/src/results/uprobe/hash-lookup-helper-gap-20261021/run-gap.py}" \
    --output "$OUT/gap" --runs 6 --iterations 50000 >"$OUT/gap.log" 2>&1
