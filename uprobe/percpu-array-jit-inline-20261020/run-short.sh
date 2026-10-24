#!/usr/bin/env bash
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/percpu-array-jit-inline-20261020
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
trap 'status=$?; printf "%s\n" "$status" >"$RESULTS/short-exit-status.txt"; date --iso-8601=seconds >"$RESULTS/short-finished-at.txt"' EXIT
date --iso-8601=seconds >"$RESULTS/short-started-at.txt"
deadline=$((SECONDS+2400))
while ! test -f "$RESULTS/build-exit-status.txt"; do
    test "$SECONDS" -lt "$deadline"
    sleep 3
done
test "$(<"$RESULTS/build-exit-status.txt")" = 0
# Refresh the test executable after the CLI traversal, without triggering
# libbpf's unconditional install-and-touch of all runtime headers again.
gmake --no-print-directory -C "$ROOT/build-jit" \
    -f runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/build.make \
    runtime/unit-test/bpftime_runtime_tests >"$RESULTS/relink-tests.log" 2>&1
timeout 180 "$ROOT/build-jit/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' \
    >"$RESULTS/tests-final.log" 2>&1
timeout 180 "$ROOT/build-jit/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' \
    >"$RESULTS/host-contract-tests.log" 2>&1
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/smoke" --runs 1 --iterations 2000 --dump-jit
python3 -u "$RESULTS/run-matched.py" --output "$RESULTS/ab6" --runs 6 --iterations 50000
python3 -u "$RESULTS/run-ordinary.py" --output "$RESULTS/ordinary-ab3" --runs 3 --iterations 50000
printf 'Per-CPU consuming workload smoke and six-round A/B completed.\n'
