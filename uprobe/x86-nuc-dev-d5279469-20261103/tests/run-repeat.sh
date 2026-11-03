#!/usr/bin/env bash
# x86 NUC, 11/03: the rewritten hash map's concurrency stress tests (1.5 s each, they lean on
# memory ordering, so far only run on aarch64) 20 times each, one process per run, with -s so
# that the counts the tests print (lookups, misses, torn copies) are kept; then the three
# churn tests 20 times more with CPU load on every CPU (stress-ng, nice 19). Each run has its
# own timeout. Usage: run-repeat.sh <build-dir>
set -uo pipefail
BUILD=${1:?build dir}
BIN=$BUILD/runtime/unit-test/bpftime_runtime_tests
ROOT=/home/y/src/bpftime-wt-uprobe-177a1dc
OUT=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-d5279469-20261103/tests/repeat
mkdir "$OUT" || exit 1
cd "$ROOT"
STRESS=
trap 'test -n "$STRESS" && kill "$STRESS" 2>/dev/null' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
# rep <name> <count> <test case name>
rep() {
    local name=$1 n=$2 tc=$3 pass=0 fail=0 rc
    mkdir "$OUT/$name"
    for i in $(seq "$n"); do
        env -u BPFTIME_ARRAY_JIT_INLINE timeout 120 "$BIN" -s "$tc" >"$OUT/$name/run-$i.log" 2>&1
        rc=$?
        if [ "$rc" -eq 0 ]; then pass=$((pass + 1)); else fail=$((fail + 1)); echo "$name run $i rc=$rc" >>"$OUT/failures.txt"; fi
    done
    echo "$name: $n runs, $pass passed, $fail failed ($tc)" | tee -a "$OUT/summary.txt"
}
CHURN="hash map lookups never miss keys present while their chains churn"
VALUE="hash map copies of a value being updated are whole"
BEHIND="hash map copies of keys updated behind the head are whole"
REUSE="hash map lookups never return a value reused for another key"
rep churn 20 "$CHURN"
rep value 20 "$VALUE"
rep behind 20 "$BEHIND"
rep reuse 20 "$REUSE"
nice -n 19 stress-ng --cpu "$(nproc)" --timeout 1800 >"$OUT/stress-ng.log" 2>&1 &
STRESS=$!
sleep 2
rep churn-loaded 20 "$CHURN"
rep value-loaded 20 "$VALUE"
rep behind-loaded 20 "$BEHIND"
kill "$STRESS"; wait "$STRESS" 2>/dev/null; STRESS=
