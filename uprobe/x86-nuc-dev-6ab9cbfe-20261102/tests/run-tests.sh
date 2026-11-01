#!/usr/bin/env bash
# x86 NUC, 11/02: tests for dev 6ab9cbfe (no timing). Daily filter, every runtime and Frida
# case in its own process (run-each-case.py), the syscall trace attach suite, and the x86
# syscall rewrite (text segment transformer) suite three times as root with
# vm.mmap_min_addr=0 (restored afterwards). Each run has its own timeout.
# Usage: run-tests.sh <build-dir>
set -uo pipefail
BUILD=${1:?build dir}
ROOT=/home/y/src/bpftime-wt-uprobe-177a1dc
OUT=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-6ab9cbfe-20261102/tests
trap 'exit 130' INT
trap 'exit 143' TERM
cd "$ROOT"
# run <name> <timeout-seconds> <command...>
run() {
    local name=$1 secs=$2; shift 2
    date --iso-8601=seconds >"$OUT/$name.started-at.txt"
    env -u BPFTIME_ARRAY_JIT_INLINE timeout "$secs" "$@" >"$OUT/$name.log" 2>&1
    printf '%s\n' "$?" >"$OUT/$name.exit-status.txt"
}
run tests-filter 1800 "$BUILD/runtime/unit-test/bpftime_runtime_tests" "$(cat "$OUT/test-filter.txt")"
run tests-runtime-each 7200 python3 "$OUT/run-each-case.py" "$BUILD/runtime/unit-test/bpftime_runtime_tests" runtime-each
run tests-frida-each 3600 python3 "$OUT/run-each-case.py" "$BUILD/attach/frida_uprobe_attach_impl/bpftime_frida_uprobe_attach_tests" frida-each
run tests-syscall-trace 1800 "$BUILD/attach/syscall_trace_attach_impl/bpftime_syscall_trace_attach_tests"

old=$(cat /proc/sys/vm/mmap_min_addr)
echo "before=$old" >"$OUT/mmap_min_addr.txt"
trap 'sudo -n sysctl -q vm.mmap_min_addr="$old"' EXIT
sudo -n sysctl -q vm.mmap_min_addr=0
echo "during=$(cat /proc/sys/vm/mmap_min_addr)" >>"$OUT/mmap_min_addr.txt"
for i in 1 2 3; do
    run "tests-transformer-$i" 600 sudo -n "$BUILD/attach/text_segment_transformer/bpftime_text_segment_transformer_tests"
done
sudo -n sysctl -q vm.mmap_min_addr="$old"
echo "after=$(cat /proc/sys/vm/mmap_min_addr)" >>"$OUT/mmap_min_addr.txt"
