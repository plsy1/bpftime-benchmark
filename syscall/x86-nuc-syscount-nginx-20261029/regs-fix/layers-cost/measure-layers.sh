#!/usr/bin/env bash
# x86 NUC, 10/29 night: cost of the x86 hook register fix on the write loop.
# Jetson's run_layers.py (the copy in ../../../x86-nuc-breakdown-20261029, same
# src/) for build-59e5e154 (hook without register save) and build-c23442ac
# (argument registers restored, XSAVE/XRSTOR mask 0xe7, red zone skipped), in the
# order A B A B, 5 rounds each, all 9 legs. Then perf (dispatch-fix profile.sh,
# 20M writes) of the rewrite leg for both. CPU 2 pinned at 1.7 GHz, turbo off,
# as the 10/29 afternoon breakdown (freq.sh of ../../../x86-nuc-syscall-session-20261029;
# only CPU 2 is pinned here). vm.mmap_min_addr = 0. Both restored on exit.
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
SYS=/home/y/src/bpftime-wt-results-x86/syscall
F=$SYS/x86-nuc-syscall-session-20261029
BD=$SYS/x86-nuc-breakdown-20261029
PERF=$SYS/x86-nuc-dispatch-fix-20261029/perf/profile.sh
T=/home/y/src/bpftime-wt-uprobe-177a1dc
CPU=2 KHZ=1700000 LOGCPU=15
TSC=$(sed -n 's/^tsc_mhz=//p' "$F/machine.txt")
STATE=$D/freq-before.txt
MMAP_ORIG=$(cat /proc/sys/vm/mmap_min_addr)
FREQLOG_PID=
finish()
{
    status=$?
    trap - EXIT
    if [ -n "$FREQLOG_PID" ]; then sudo -n kill "$FREQLOG_PID" 2>/dev/null || true; fi
    sudo -n "$T/build-c23442ac/tools/bpftimetool/bpftimetool" remove >/dev/null 2>&1 || true
    echo "$MMAP_ORIG" | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null || status=98
    echo "vm.mmap_min_addr original $MMAP_ORIG, after restore $(cat /proc/sys/vm/mmap_min_addr)" >"$D/mmap_min_addr.txt"
    if [ -e "$STATE" ]; then "$F/freq.sh" restore "$CPU" "$STATE" || status=99; fi
    "$F/freq.sh" show "$CPU" >"$D/freq-restored.txt" 2>&1 || true
    printf '%s\n' "$status" >"$D/exit-status.txt"
    date --iso-8601=seconds >"$D/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$D/started-at.txt"
test ! -e /dev/shm/bpftime_maps_shm
ps -eo pid,pcpu,etime,args --sort=-pcpu | head -10 | cut -c1-200 >"$D/ps-before.txt"
"$F/freq.sh" pin "$CPU" "$KHZ" "$STATE" turbo-off
"$F/freq.sh" show "$CPU" >"$D/freq-pinned.txt"
sudo -n modprobe msr
for try in $(seq 15); do
    sudo -n python3 "$F/busy-check.py" "$TSC" "$CPU:$KHZ" >>"$D/freq-pinned.txt" && break
    test "$try" -lt 15
done
# (run of 10/29: the arguments were given as "$CPU" "$LOGCPU" "$TSC", in the order of
# the single-CPU freq-log.py; this copy is the multi-CPU one and exited at once.)
sudo -n python3 "$F/freq-log.py" "$LOGCPU" "$TSC" "$CPU" >"$D/freq-log.txt" 2>&1 &
FREQLOG_PID=$!
echo 0 | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null
n=0
for b in 59e5e154 c23442ac 59e5e154 c23442ac; do
    n=$((n + 1))
    out=$D/run$n-$b
    sudo -n python3 "$F/busy-check.py" "$TSC" "$CPU:$KHZ" >"$out.freq-before.txt"
    echo "run$n-$b starts at log line $(wc -l <"$D/freq-log.txt")" >>"$D/freq-log-marks.txt"
    (cd "$BD" && BPFTIME_BUILD=$T/build-$b taskset -c "$CPU" \
        python3 run_layers.py --rounds 5 --out "$out.jsonl" >"$out.log" 2>&1)
    python3 "$BD/summarize.py" "$out.jsonl" >"$out.summary.md"
done
mkdir -p "$D/perf"
for b in 59e5e154 c23442ac; do
    mkdir "$D/perf/$b"
    sudo -n python3 "$F/busy-check.py" "$TSC" "$CPU:$KHZ" >"$D/perf/$b/rewrite.freq-before.txt"
    echo "perf-$b starts at log line $(wc -l <"$D/freq-log.txt")" >>"$D/freq-log-marks.txt"
    OUT=$D/perf/$b BPFTIME_BUILD=$T/build-$b taskset -c "$CPU" "$PERF" rewrite >"$D/perf/$b/rewrite.console.txt" 2>&1
done
sudo -n python3 "$F/busy-check.py" "$TSC" "$CPU:$KHZ" >"$D/freq-after.txt"
