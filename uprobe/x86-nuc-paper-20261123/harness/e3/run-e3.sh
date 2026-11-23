#!/usr/bin/env bash
# Paper E3 (x86): official uprobe suite (benchmark.py --iter 10) for O and D in ABBA order,
# each run with its own kernel group, plus kernel-only bookends before, between and after.
# CPU 2 pinned at 4.4 GHz (turbo on, C6+ off) as in x86-nuc-dev-d5279469-20261103/full10.
# Usage: run-e3.sh <session-id> <legs...>   (e.g. official dev dev official)
set -euo pipefail
SID=$1; shift
R=/home/y/src/bpftime-x86-runs/paper-20261124
E=$R/harness/e3
F=$R/raw/$SID
CPU=2; KHZ=4400000
mkdir "$F"
STATE=$F/freq-before.txt
FREQLOG_PID=
finish() {
    status=$?; trap - EXIT
    [ -n "$FREQLOG_PID" ] && sudo -n kill "$FREQLOG_PID" 2>/dev/null || true
    for a in official dev opc dpc; do sudo -n $R/runners/$a/bpftime/build/tools/bpftimetool/bpftimetool remove >>"$F/cleanup.log" 2>&1 || true; done
    [ -e "$STATE" ] && { "$E/freq.sh" restore "$CPU" "$STATE" || status=99; }
    "$E/freq.sh" show "$CPU" >"$F/freq-restored.txt" 2>&1 || true
    sudo -n chown -R "$(id -u):$(id -g)" "$F" 2>/dev/null || true
    printf '%s\n' "$status" >"$F/exit-status.txt"; date --iso-8601=seconds >"$F/finished-at.txt"; exit "$status"
}
trap finish EXIT; trap 'exit 130' INT; trap 'exit 143' TERM
date --iso-8601=seconds >"$F/started-at.txt"
(cd "$R/bench" && sha256sum --check --quiet sha256.txt)
for a in official dev; do (cd "$R/builds/$a" && sha256sum --check --quiet sha256.txt); done
test -z "$(find /dev/shm -mindepth 1 -name 'bpftime*')"
awk '{ if ($1 >= 0.3) { print "load too high: " $1; exit 1 } }' /proc/loadavg
"$E/freq.sh" pin "$CPU" "$KHZ" "$STATE" turbo-on
for try in $(seq 15); do python3 "$E/busy-freq.py" "$CPU" "$KHZ" >>"$F/freq-pinned.txt" && break; test "$try" -lt 15; done
top -bn2 -d2 | awk '/^top -/{n++} n==2' | sed -n '1,15p' >"$F/top-before.txt"
sudo -n modprobe msr
sudo -n python3 "$E/freq-log.py" "$CPU" 15 "$(sed -n 's/^tsc_mhz=//p' "$E/machine.txt")" >"$F/freq-log.txt" 2>&1 &
FREQLOG_PID=$!
bookend() {
    local n=$1; echo "bookend $n at log line $(wc -l <"$F/freq-log.txt")" >>"$F/marks.txt"
    cd "$R/runners/official/bpftime"
    sudo -n timeout 1200 taskset -c "$CPU" env -u BPFTIME_ARRAY_JIT_INLINE PYTHONUNBUFFERED=1 python3 -u "$E/kernel_bookend.py" 10 "$F/bookend-$n.json" >"$F/bookend-$n.log" 2>&1
}
i=0
bookend 0
for a in "$@"; do
    i=$((i + 1)); OUT="$F/run$i-$a"; mkdir "$OUT"
    echo "run$i $a at log line $(wc -l <"$F/freq-log.txt")" >>"$F/marks.txt"
    date --iso-8601=seconds >"$OUT/started-at.txt"
    cd "$R/runners/$a/bpftime"
    sudo -n timeout 2400 taskset -c "$CPU" env -u BPFTIME_ARRAY_JIT_INLINE PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
        python3 -u benchmark/uprobe/benchmark.py --iter 10 >"$OUT/runner.log" 2>&1
    for x in benchmark-output.json results.md benchmark-logs.txt; do test "benchmark/uprobe/$x" -nt "$OUT/started-at.txt"; cp "benchmark/uprobe/$x" "$OUT/"; done
    sudo -n build/tools/bpftimetool/bpftimetool remove >"$OUT/post-cleanup.log" 2>&1 || true
    python3 -I - "$OUT/benchmark-output.json" <<'PY'
import json, math, sys
r = json.load(open(sys.argv[1]))
for g, c in [('kernel_uprobe', 17), ('userspace_uprobe', 17), ('embed', 1)]:
    assert len(r[g]) == c, (g, len(r[g]))
    for n, v in r[g].items():
        assert len(v['raw_values']) == 10 and all(math.isfinite(x) and x > 0 for x in v['raw_values']), (g, n)
PY
    [ $((i % 2)) -eq 0 ] && bookend $i
done
[ $((i % 2)) -eq 0 ] || bookend $i
python3 "$E/busy-freq.py" "$CPU" "$KHZ" >"$F/freq-after.txt" || echo FAIL >>"$F/freq-after.txt"
