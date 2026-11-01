#!/usr/bin/env bash
# x86 NUC, 11/02: dev 6ab9cbfe cross-check, one measurement after another: the uprobe
# event audit (unpinned, no timing), the complete ten-round uprobe suite (full10/), then
# the ssl-nginx fair round (ssl-nginx/). Each step has its own total timeout. plymouthd
# (about 2.5% CPU since boot) is moved to E cores 12-14 for the session and moved back.
# Docker stays as found (it now runs the user's sing-box container); only this run's
# container is removed, by ssl-nginx/run.sh.
set -u
D=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-dev-6ab9cbfe-20261102
L=$D/session
mkdir "$L" || exit 1
date --iso-8601=seconds >"$L/started-at.txt"
uname -r >"$L/kernel.txt"
# Nothing of an earlier run may be left.
pgrep -af 'benchmark/test|uprobe/uprobe|sslsniff|nginx|wrk|bpftime_' | grep -v pgrep >"$L/leftover-processes.txt"
if [ -s "$L/leftover-processes.txt" ] || [ -e /dev/shm/bpftime_maps_shm ]; then
    echo "leftover processes or shared memory" >&2; exit 1
fi
PLY=$(pgrep -x plymouthd || true)
if [ -n "$PLY" ]; then
    taskset -pc "$PLY" >"$L/plymouthd-affinity.txt"
    sudo -n taskset -pc 12-14 "$PLY" >>"$L/plymouthd-affinity.txt"
fi
restore_ply() { if [ -n "$PLY" ]; then sudo -n taskset -pc 0-15 "$PLY" >>"$L/plymouthd-affinity.txt" 2>&1; fi; }
trap restore_ply EXIT

timeout 900 python3 "$D/full10/audit-events.py" 2 >"$D/full10/audit-events.log" 2>&1
echo "audit exit $?" | tee -a "$L/steps.txt"
date --iso-8601=seconds >>"$L/steps.txt"
timeout 3600 "$D/full10/run-full.sh" 4400000 turbo-on full10-4.4ghz-turbo >"$D/full10/full-console.log" 2>&1
echo "full10 exit $?" | tee -a "$L/steps.txt"
date --iso-8601=seconds >>"$L/steps.txt"
pgrep -af 'benchmark/test|uprobe/uprobe|bpftime_' | grep -v pgrep >"$L/leftover-after-full10.txt"
timeout 5400 "$D/ssl-nginx/run.sh" >"$D/ssl-nginx/run.console.log" 2>&1
echo "ssl-nginx exit $?" | tee -a "$L/steps.txt"
sudo -n docker ps -a --format '{{.Names}}' | grep -x ssl-x86-6ab9 && sudo -n docker rm -f ssl-x86-6ab9
pgrep -af 'sslsniff|nginx|wrk' | grep -v pgrep >"$L/leftover-after-ssl.txt"
date --iso-8601=seconds >"$L/finished-at.txt"
