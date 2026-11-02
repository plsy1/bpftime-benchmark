#!/usr/bin/env bash
# Driver for run.sh: refuse to start if anything of an earlier run is left, move plymouthd to
# E cores 12-14 for the run (as ../../run-all.sh) and back, total timeout one hour.
# The process check skips this directory's own scripts (their path contains "ssl-nginx").
set -u
A=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-6ab9cbfe-20261102/ssl-nginx/perf-lost-audit
pgrep -af 'benchmark/test|uprobe/uprobe|sslsniff|nginx|wrk|bpftime_' | grep -v -e pgrep -e "$A/" >"$A/leftover-processes.txt"
if [ -s "$A/leftover-processes.txt" ] || [ -e /dev/shm/bpftime_maps_shm ]; then
    echo "leftover processes or shared memory" >&2; exit 1
fi
PLY=$(pgrep -x plymouthd || true)
if [ -n "$PLY" ]; then
    taskset -pc "$PLY" >"$A/plymouthd-affinity.txt"
    sudo -n taskset -pc 12-14 "$PLY" >>"$A/plymouthd-affinity.txt"
fi
restore_ply() { if [ -n "$PLY" ]; then sudo -n taskset -pc 0-15 "$PLY" >>"$A/plymouthd-affinity.txt" 2>&1; fi; }
trap restore_ply EXIT
timeout 3600 "$A/run.sh" >"$A/run.console.log" 2>&1
echo "run.sh exit $?" | tee "$A/session-exit.txt"
pgrep -af 'sslsniff|nginx|wrk' | grep -v -e pgrep -e "$A/" >"$A/leftover-after.txt"
