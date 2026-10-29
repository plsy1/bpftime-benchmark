#!/usr/bin/env bash
# x86 NUC, 10/29 night: run_correct.py --rounds 10 (Jetson's, unchanged) in the
# container ngx-x86 (--cpuset-cpus 0-7, working directory regs-fix/view-e299cd3f,
# build-e299cd3f), CPUs 0-7 pinned at 1.7 GHz with turbo off exactly as
# official.sh, vm.mmap_min_addr = 0 for zpoline. Both restored on exit (turbo
# back on first). Start from a copy in a path without the words syscount /
# syscall / bpftime / victim, with no arguments.
set -euo pipefail
X=$(cd "$(dirname "$0")" && pwd)
case "$X" in *syscount*|*syscall*|*bpftime*|*victim*) echo "run from a clean path" >&2; exit 2 ;; esac
N=/home/y/src/bpftime-wt-results-x86/syscall/x86-nuc-syscount-nginx-20261029
OF=$N/regs-fix/correct
CPUS="0 1 2 3 4 5 6 7"
KHZ=1700000
P=/sys/devices/system/cpu
TSC=$(sed -n 's/^tsc_mhz=//p' "$X/machine.txt")
STATE=$OF/pin-state.txt
FREQLOG_PID=
MMAP_ORIG=$(cat /proc/sys/vm/mmap_min_addr)
finish()
{
    status=$?
    trap - EXIT
    if [ -n "$FREQLOG_PID" ]; then sudo -n kill "$FREQLOG_PID" 2>/dev/null || true; fi
    echo "$MMAP_ORIG" | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null || status=98
    echo "vm.mmap_min_addr original $MMAP_ORIG, after restore $(cat /proc/sys/vm/mmap_min_addr)" >"$OF/mmap_min_addr.txt"
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
echo 0 | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null
timeout 7200 sudo -n docker exec ngx-x86 python3 ../../run_correct.py --rounds 10 \
    --out ../correct/correct-run.jsonl >"$OF/run.log" 2>&1 || echo "exit $?" >>"$OF/run.log"
sudo -n python3 "$X/busy-check.py" "$TSC" "${args[@]}" >"$OF/busy-check-after.txt" || true
sudo -n chown -R y:y "$OF"
