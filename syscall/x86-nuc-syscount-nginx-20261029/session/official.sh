#!/usr/bin/env bash
# x86 NUC, 10/29 evening: the official benchmark.py (copied unchanged as
# view/runbench.py) in the container ngx-x86 (--cpuset-cpus 0-7), with CPUs 0-7
# (the four P cores, both threads) pinned at 1.7 GHz, turbo off: pin-cpus.sh
# (min = max, C6+ off; same as the ssl-nginx sweep) then no_turbo = 1; busy check
# on all eight until it passes, before and after; real frequency logged once per
# second from E core 15. Restored on exit (turbo back on first, so that the
# original scaling_max_freq can be written). vm.mmap_min_addr is not touched:
# benchmark.py's bpftime legs preload the agent only, no rewriting.
# The script's cleanup runs `sudo pkill -f` on the tool's name, which reaches host
# processes (--pid=host): start this from a copy in a path without the words
# syscount / syscall / bpftime / victim, with no arguments.
set -euo pipefail
X=$(cd "$(dirname "$0")" && pwd)
case "$X" in *syscount*|*syscall*|*bpftime*|*victim*) echo "run from a clean path" >&2; exit 2 ;; esac
N=/home/y/src/bpftime-wt-results-x86/syscall/x86-nuc-syscount-nginx-20261029
OF=$N/official
V=$N/view
CPUS="0 1 2 3 4 5 6 7"
KHZ=1700000
P=/sys/devices/system/cpu
TSC=$(sed -n 's/^tsc_mhz=//p' "$X/machine.txt")
STATE=$OF/pin-state.txt
FREQLOG_PID=
finish()
{
    status=$?
    trap - EXIT
    if [ -n "$FREQLOG_PID" ]; then sudo -n kill "$FREQLOG_PID" 2>/dev/null || true; fi
    echo 0 | sudo -n tee $P/intel_pstate/no_turbo >/dev/null || status=99
    if [ -e "$STATE" ]; then "$X/pin-cpus.sh" restore "$STATE" || status=99; fi
    "$X/pin-cpus.sh" show $CPUS >"$OF/pin-restored.txt" 2>&1 || true
    printf '%s\n' "$status" >"$OF/exit-status.txt"
    date --iso-8601=seconds >"$OF/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$OF/started-at.txt"
ps -eo pid,pcpu,etime,args --sort=-pcpu | head -12 | cut -c1-200 >"$OF/ps-before.txt"
args=(); for c in $CPUS; do args+=("$c:$KHZ"); done
"$X/pin-cpus.sh" pin "$STATE" "${args[@]}"
echo 1 | sudo -n tee $P/intel_pstate/no_turbo >/dev/null
"$X/pin-cpus.sh" show $CPUS >"$OF/pin-pinned.txt"
sudo -n modprobe msr
for try in $(seq 15); do
    sudo -n python3 "$X/busy-check.py" "$TSC" "${args[@]}" >>"$OF/busy-check-before.txt" && break
    test "$try" -lt 15
done
sudo -n python3 "$X/freq-log.py" 15 "$TSC" $CPUS >"$OF/freq-log.txt" 2>&1 &
FREQLOG_PID=$!
ls "$V" >"$X/view-before.txt"
timeout 3600 sudo -n docker exec ngx-x86 python3 runbench.py >"$OF/run.log" 2>&1 || echo "exit $?" >>"$OF/run.log"
sudo -n python3 "$X/busy-check.py" "$TSC" "${args[@]}" >"$OF/busy-check-after.txt" || true
for f in $(ls "$V"); do grep -qxF "$f" "$X/view-before.txt" || sudo -n mv "$V/$f" "$OF/"; done
sudo -n chown -R y:y "$OF"
