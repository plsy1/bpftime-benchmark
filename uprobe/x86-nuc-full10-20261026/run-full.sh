#!/usr/bin/env bash
# x86 NUC: complete ten-round official uprobe suite (benchmark.py --iter 10, kernel
# + userspace + embedded legs) for the dev branch and official 177a1dc, in that
# order, in one session. Same workloads (dev tree benchmark/), frozen runtime
# artifacts swapped per leg. CPU 2 (P core; SMT sibling 3 left idle) pinned
# (min = max, powersave governor, C6+ disabled on CPUs 2-3) for the whole
# session; after pinning, wait until a busy check passes (the first seconds
# after a change ran at 1.4 GHz), check before and after each leg, restore.
# Usage: run-full.sh <khz> <turbo-off|turbo-on> <out-name>
# Adapted from Jetson baseline-177a1dc-jit-llvm16-20261021/run-full.sh.
set -euo pipefail
D=/home/y/src/bpftime-wt-results-x86/uprobe/x86-nuc-full10-20261026
RUN=/home/y/src/bpftime-x86-runs/full10-20261026
CPU=2
KHZ=${1:?khz}
MODE=${2:?turbo-off or turbo-on}
NAME=${3:?out name}
F="$D/$NAME"
STATE="$F/freq-before.txt"
test ! -e /dev/shm/bpftime_maps_shm
mkdir "$F"
FREQLOG_PID=
finish()
{
    status=$?
    trap - EXIT
    if [ -n "$FREQLOG_PID" ]; then sudo -n kill "$FREQLOG_PID" 2>/dev/null || true; fi
    for a in dev official; do
        sudo -n "$RUN/artifacts/$a/tools/bpftimetool/bpftimetool" remove >>"$F/cleanup.log" 2>&1 || true
    done
    if [ -e "$STATE" ]; then "$D/freq.sh" restore "$CPU" "$STATE" || status=99; fi
    "$D/freq.sh" show "$CPU" >"$F/freq-restored.txt" 2>&1 || true
    printf '%s\n' "$status" >"$F/exit-status.txt"
    date --iso-8601=seconds >"$F/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$F/started-at.txt"
sha256sum --check --quiet "$RUN/artifact-sha256.txt"
"$D/freq.sh" pin "$CPU" "$KHZ" "$STATE" "$MODE"
"$D/freq.sh" show "$CPU" >"$F/freq-pinned.txt"
for try in $(seq 15); do
    python3 "$D/busy-freq.py" "$CPU" "$KHZ" >>"$F/freq-pinned.txt" && break
    test "$try" -lt 15
done
top -bn2 -d2 | awk '/^top -/{n++} n==2' | sed -n '1,15p' >"$F/top-before.txt"
# Real frequency of the measured CPU once per second, sampled from E core 15.
sudo -n modprobe msr
sudo -n python3 "$D/freq-log.py" "$CPU" 15 "$(sed -n 's/^tsc_mhz=//p' "$D/machine.txt")" >"$F/freq-log.txt" 2>&1 &
FREQLOG_PID=$!
for leg in dev official; do
    OUT="$F/$leg"
    mkdir "$OUT"
    echo "leg $leg starts at log line $(wc -l <"$F/freq-log.txt")" >>"$F/freq-log-marks.txt"
    python3 "$D/busy-freq.py" "$CPU" "$KHZ" >"$OUT/freq-before.txt"
    date --iso-8601=seconds >"$OUT/started-at.txt"
    cd "$RUN/runner-$leg/bpftime"
    sudo -n taskset -c "$CPU" env -u BPFTIME_ARRAY_JIT_INLINE PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
        /usr/bin/python3 -u benchmark/uprobe/benchmark.py --iter 10 >"$OUT/runner.log" 2>&1
    for artifact in benchmark-output.json results.md benchmark-logs.txt; do
        test "benchmark/uprobe/$artifact" -nt "$OUT/started-at.txt"
        cp "benchmark/uprobe/$artifact" "$OUT/"
    done
    sudo -n "$RUN/artifacts/$leg/tools/bpftimetool/bpftimetool" remove >"$OUT/post-cleanup.log" 2>&1
    python3 -I - "$OUT/benchmark-output.json" <<'PY'
import json,math,sys
result=json.load(open(sys.argv[1]))
for group,count in [('kernel_uprobe',17),('userspace_uprobe',17),('embed',1)]:
    assert len(result[group])==count,(group,len(result[group]))
    for name,value in result[group].items():
        assert len(value['raw_values'])==10,(group,name)
        assert all(math.isfinite(x) and x>0 for x in value['raw_values'])
print('Full-suite sample validation passed.',flush=True)
PY
    "$D/freq.sh" show "$CPU" >"$OUT/freq-after.txt"
    python3 "$D/busy-freq.py" "$CPU" "$KHZ" >>"$OUT/freq-after.txt"
    date --iso-8601=seconds >"$OUT/finished-at.txt"
    echo "leg $leg ends at log line $(wc -l <"$F/freq-log.txt")" >>"$F/freq-log-marks.txt"
done
sha256sum --check --quiet "$RUN/artifact-sha256.txt"
