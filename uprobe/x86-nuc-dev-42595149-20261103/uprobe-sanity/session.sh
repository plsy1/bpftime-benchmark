#!/usr/bin/env bash
# Driver for run-sanity.sh (as ../../x86-nuc-dev-6ab9cbfe-20261102/run-all.sh): refuse to start
# if anything of an earlier run is left, move plymouthd to E cores 12-14 for the run and back,
# total timeout 40 minutes. The process check skips this directory's own scripts.
set -u
D=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-42595149-20261103/uprobe-sanity
pgrep -af 'benchmark/test|uprobe/uprobe|sslsniff|nginx|wrk|bpftime_' | grep -v -e pgrep -e "$D/" >"$D/leftover-processes.txt"
if [ -s "$D/leftover-processes.txt" ] || [ -e /dev/shm/bpftime_maps_shm ]; then
    echo "leftover processes or shared memory" >&2; exit 1
fi
PLY=$(pgrep -x plymouthd || true)
if [ -n "$PLY" ]; then
    taskset -pc "$PLY" >"$D/plymouthd-affinity.txt"
    sudo -n taskset -pc 12-14 "$PLY" >>"$D/plymouthd-affinity.txt"
fi
restore_ply() { if [ -n "$PLY" ]; then sudo -n taskset -pc 0-15 "$PLY" >>"$D/plymouthd-affinity.txt" 2>&1; fi; }
trap restore_ply EXIT
timeout 2400 "$D/run-sanity.sh" 4400000 turbo-on sanity-4.4ghz-turbo >"$D/sanity-console.log" 2>&1
echo "run-sanity.sh exit $?" | tee "$D/session-exit.txt"
pgrep -af 'benchmark/test|uprobe/uprobe|bpftime_' | grep -v -e pgrep -e "$D/" >"$D/leftover-after.txt"
