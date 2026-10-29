#!/usr/bin/env bash
# x86 NUC, 10/29 (copied for the dispatch fix 2e850c45: only BUILD and the output
# directories changed): one pinned session = the layer breakdown (run_layers.py, 5 rounds)
# then the official benchmark/syscall (benchmark.py unmodified, 10 runs per leg),
# vm.mmap_min_addr = 0 for zpoline. CPUs 2 and 4 (P cores; SMT siblings 3 and 5
# left idle) pinned at <khz> (min = max, powersave governor, C6+ disabled on the
# core's both threads; freq.sh). The breakdown runs on CPU 2 alone. The official
# script runs on CPUs 2 and 4: its loader (syscall.c) busy-waits in
# `while (!exiting);` for the whole leg, and on a single CPU it took half of the
# victim's time (first attempt, ../*/4.4ghz-turbo-onecpu: every traced leg doubled).
# Frequency and mmap_min_addr are restored on exit; the original values are
# recorded in the session dir.
#
# Must be started from a copy outside any path containing the words victim,
# syscall or bpftime: benchmark.py's cleanup kill -9s every process whose command
# line contains one of them (its parents and the frequency logger included).
# Hence also no output directory on the command line.
#   cp measure.sh freq.sh busy-check.py freq-log.py machine.txt <clean-dir>/
#   bash <clean-dir>/measure.sh <khz> <turbo-on|turbo-off> <config-name>
set -euo pipefail
KHZ=${1:?khz}
MODE=${2:?turbo-on or turbo-off}
NAME=${3:?config name}
CPU=2
CPU2=4
LOGCPU=15
X=$(cd "$(dirname "$0")" && pwd)
R=/home/y/src/bpftime-wt-results-x86/syscall
SRCTREE=/home/y/src/bpftime-wt-uprobe-177a1dc
BUILD=$SRCTREE/build-2e850c45
S=$R/x86-nuc-dispatch-fix-20261029/session/$NAME
BD=$R/x86-nuc-dispatch-fix-20261029/breakdown/$NAME
OF=$R/x86-nuc-dispatch-fix-20261029/official/$NAME
TSC=$(sed -n 's/^tsc_mhz=//p' "$X/machine.txt")
mkdir "$S" "$BD" "$OF" "$X/view-$NAME" "$X/emptyhome-$NAME"
case "$X" in *victim*|*syscall*|*bpftime*) echo "run from a clean path" >&2; exit 2 ;; esac
test ! -e /dev/shm/bpftime_maps_shm

FREQLOG_PID=
MMAP_ORIG=$(cat /proc/sys/vm/mmap_min_addr)
STATE=$S/freq-before.txt
STATE2=$S/freq-before-cpu$CPU2.txt
finish()
{
    status=$?
    trap - EXIT
    if [ -n "$FREQLOG_PID" ]; then sudo -n kill "$FREQLOG_PID" 2>/dev/null || true; fi
    sudo -n "$BUILD/tools/bpftimetool/bpftimetool" remove >>"$S/cleanup.log" 2>&1 || true
    echo "$MMAP_ORIG" | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null || status=98
    echo "vm.mmap_min_addr after restore: $(cat /proc/sys/vm/mmap_min_addr)" >>"$S/mmap_min_addr.txt"
    # CPU2 first: its state file was written after CPU's pin changed no_turbo.
    if [ -e "$STATE2" ]; then "$X/freq.sh" restore "$CPU2" "$STATE2" || status=99; fi
    if [ -e "$STATE" ]; then "$X/freq.sh" restore "$CPU" "$STATE" || status=99; fi
    # With turbo-off, CPU's pin clamps every CPU's scaling_max_freq to the base
    # frequency before STATE2 is written; put CPU2's limits back from the values
    # recorded before any pin (the 1.7 GHz session left cpu4 at 1700000 without this).
    if [ -e "$S/freq-orig-cpu$CPU2.txt" ]; then
        for f in scaling_max_freq scaling_min_freq; do
            sed -n "s/^cpu$CPU2.$f=//p" "$S/freq-orig-cpu$CPU2.txt" |
                sudo -n tee /sys/devices/system/cpu/cpu$CPU2/cpufreq/$f >/dev/null || status=99
        done
    fi
    "$X/freq.sh" show "$CPU" >"$S/freq-restored.txt" 2>&1 || true
    "$X/freq.sh" show "$CPU2" >>"$S/freq-restored.txt" 2>&1 || true
    printf '%s\n' "$status" >"$S/exit-status.txt"
    date --iso-8601=seconds >"$S/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$S/started-at.txt"
{
    echo "args: $KHZ $MODE $NAME, cpus $CPU (breakdown) and $CPU,$CPU2 (official), freq logger on cpu $LOGCPU"
    cat "$X/machine.txt"
    echo "uname: $(uname -a)"
    echo "dev commit: $(git -C "$SRCTREE" rev-parse HEAD)"
    echo "build dir: $BUILD"
    echo "compiler (bpftime): $(sed -n 's/^CMAKE_CXX_COMPILER:[A-Z]*=//p' "$BUILD/CMakeCache.txt") $(g++-13 --version | head -1)"
    echo "LLVM: $(sed -n 's/^LLVM_DIR:[A-Z]*=//p' "$BUILD/CMakeCache.txt"), $(clang-19 --version | head -1)"
    echo "workload compiler: $(gcc --version | head -1)"
    echo "mitigations: $(grep -r . /sys/devices/system/cpu/vulnerabilities/ | sed 's|.*/||' | tr '\n' ';')"
} >"$S/env.txt"
echo "vm.mmap_min_addr original: $MMAP_ORIG" >"$S/mmap_min_addr.txt"

"$X/freq.sh" show "$CPU2" >"$S/freq-orig-cpu$CPU2.txt"
"$X/freq.sh" pin "$CPU" "$KHZ" "$STATE" "$MODE"
"$X/freq.sh" pin "$CPU2" "$KHZ" "$STATE2" "$MODE"
"$X/freq.sh" show "$CPU" >"$S/freq-pinned.txt"
"$X/freq.sh" show "$CPU2" >>"$S/freq-pinned.txt"
sudo -n modprobe msr
for try in $(seq 15); do
    sudo -n python3 "$X/busy-check.py" "$TSC" "$CPU:$KHZ" "$CPU2:$KHZ" >>"$S/freq-pinned.txt" && break
    test "$try" -lt 15
done
top -bn2 -d2 | awk '/^top -/{n++} n==2' | sed -n '1,15p' >"$S/top-before.txt"
sudo -n python3 "$X/freq-log.py" "$LOGCPU" "$TSC" "$CPU" "$CPU2" >"$S/freq-log.txt" 2>&1 &
FREQLOG_PID=$!
echo 0 | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null
echo "vm.mmap_min_addr during the session: $(cat /proc/sys/vm/mmap_min_addr)" >>"$S/mmap_min_addr.txt"

# 1. layer breakdown
echo "breakdown starts at log line $(wc -l <"$S/freq-log.txt")" >>"$S/freq-log-marks.txt"
sudo -n python3 "$X/busy-check.py" "$TSC" "$CPU:$KHZ" "$CPU2:$KHZ" >"$BD/freq-before.txt"
date --iso-8601=seconds >"$BD/started-at.txt"
(cd "$R/x86-nuc-breakdown-20261029" && BPFTIME_BUILD=$BUILD taskset -c "$CPU" \
    python3 run_layers.py --rounds 5 --out "$BD/run.jsonl" >"$BD/run.log" 2>&1)
date --iso-8601=seconds >"$BD/finished-at.txt"
sudo -n python3 "$X/busy-check.py" "$TSC" "$CPU:$KHZ" "$CPU2:$KHZ" >"$BD/freq-after.txt"
python3 "$R/x86-nuc-breakdown-20261029/summarize.py" "$BD/run.jsonl" >"$BD/summary.md"

# 2. official benchmark/syscall from a symlinked view, script copied as runbench.py
V=$X/view-$NAME
ln -s "$SRCTREE/benchmark" "$V/benchmark"
ln -s "$BUILD" "$V/build"
cp "$SRCTREE/benchmark/syscall/benchmark.py" "$V/runbench.py"
ls "$SRCTREE/benchmark/syscall" | grep '^syscall_benchmark_results_\|^results.md$' >"$X/json-before-$NAME.txt" || true
echo "official starts at log line $(wc -l <"$S/freq-log.txt")" >>"$S/freq-log-marks.txt"
sudo -n python3 "$X/busy-check.py" "$TSC" "$CPU:$KHZ" "$CPU2:$KHZ" >"$OF/freq-before.txt"
date --iso-8601=seconds >"$OF/started-at.txt"
(cd "$V" && timeout 1800 sudo -n env HOME="$X/emptyhome-$NAME" PYTHONUNBUFFERED=1 taskset -c "$CPU,$CPU2" \
    python3 runbench.py >"$OF/run.log" 2>&1) || echo "exit $?" >>"$OF/run.log"
date --iso-8601=seconds >"$OF/finished-at.txt"
sudo -n python3 "$X/busy-check.py" "$TSC" "$CPU:$KHZ" "$CPU2:$KHZ" >"$OF/freq-after.txt"
for f in $(ls "$SRCTREE/benchmark/syscall" | grep '^syscall_benchmark_results_\|^results.md$'); do
    grep -qxF "$f" "$X/json-before-$NAME.txt" || mv "$SRCTREE/benchmark/syscall/$f" "$OF/"
done
echo "end at log line $(wc -l <"$S/freq-log.txt")" >>"$S/freq-log-marks.txt"
