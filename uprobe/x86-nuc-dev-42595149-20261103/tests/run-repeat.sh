#!/usr/bin/env bash
# x86 NUC, 11/03: the Jetson session asked to watch the three timing ("waits / does not wait
# for a lock") cases in [percpu-locks] on x86. Run each new tag of 0ffe368b..42595149 many
# times, one process per run, nothing else running: [percpu-locks] 50 times; [value-access],
# [home-stripe], [spin-lock] and the hash map [capacity] churn test 20 times each; then
# [percpu-locks] 20 more times with CPU load on every CPU (stress-ng, nice 19) as a harsher
# schedule. Each run has its own timeout.
# Usage: run-repeat.sh <build-dir>
set -uo pipefail
BUILD=${1:?build dir}
BIN=$BUILD/runtime/unit-test/bpftime_runtime_tests
ROOT=/home/y/src/bpftime-wt-uprobe-177a1dc
OUT=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-42595149-20261103/tests/repeat
mkdir "$OUT" || exit 1
cd "$ROOT"
STRESS=
trap 'test -n "$STRESS" && kill "$STRESS" 2>/dev/null' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
# rep <name> <count> <tag>
rep() {
    local name=$1 n=$2 tag=$3 pass=0 fail=0 rc
    for i in $(seq "$n"); do
        # Only a failing run's output is kept.
        env -u BPFTIME_ARRAY_JIT_INLINE timeout 300 "$BIN" "$tag" >"$OUT/last.log" 2>&1
        rc=$?
        if [ "$rc" -eq 0 ]; then pass=$((pass + 1)); else fail=$((fail + 1)); cp "$OUT/last.log" "$OUT/$name-$i.log"; echo "$name run $i rc=$rc" >>"$OUT/failures.txt"; fi
    done
    echo "$name $tag: $n runs, $pass passed, $fail failed" | tee -a "$OUT/summary.txt"
}
rep percpu-locks 50 '[percpu-locks]'
rep value-access 20 '[value-access]'
rep home-stripe 20 '[home-stripe]'
rep spin-lock 20 '[spin-lock]'
rep capacity 20 '[bpftime_hash_map][capacity]'
nice -n 19 stress-ng --cpu "$(nproc)" --timeout 1800 >"$OUT/stress-ng.log" 2>&1 &
STRESS=$!
sleep 2
rep percpu-locks-loaded 20 '[percpu-locks]'
kill "$STRESS"; wait "$STRESS" 2>/dev/null; STRESS=
