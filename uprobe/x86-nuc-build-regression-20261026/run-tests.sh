#!/usr/bin/env bash
# x86 NUC: daily test filter, the complete runtime suite and the Frida attach
# suite for one built tree. Usage: run-tests.sh <label> <source-root>
set -uo pipefail
LABEL=${1:?label}
ROOT=${2:?source root}
D=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-build-regression-20261026
OUT=$D/$LABEL
BUILD="$ROOT/build-jit"
trap 'exit 130' INT
trap 'exit 143' TERM
FILTER=$(cat "$D/test-filter.txt")
cd "$ROOT"
run() {
    local name=$1; shift
    date --iso-8601=seconds >"$OUT/$name.started-at.txt"
    env -u BPFTIME_ARRAY_JIT_INLINE timeout 1800 "$@" >"$OUT/$name.log" 2>&1
    printf '%s\n' "$?" >"$OUT/$name.exit-status.txt"
}
run tests-filter "$BUILD/runtime/unit-test/bpftime_runtime_tests" "$FILTER"
run tests-runtime-all "$BUILD/runtime/unit-test/bpftime_runtime_tests"
run tests-frida "$BUILD/attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests"
