#!/usr/bin/env bash
# x86 NUC, 11/04 (Jetson session): the malloc example without its bpf_printk (ex-noprintk/); otherwise as run.sh:
# x86 NUC, 11/04 (Jetson session): multi-thread real-tool throughput, the x86 counterpart of Jetson
# results uprobe/realtools-20261104/measure-mt, for the two tools built here (bpftime examples
# malloc and funclatency, built from dev 7ad5a152's tree with clang-19): none, kernel, dev
# a4ac0d31 (frozen x86 build of 11/02), dev 7ad5a152; 1/2/4/6 workload threads pinned to CPUs
# 5..0 (P cores, 3.5 GHz, turbo on, min = max, C6 and deeper off, as keypattern/run.sh).
set -euo pipefail
R=/home/y/src/bpftime-x86-runs/r1-miss-20261104/realtools
K=/home/y/src/bpftime-x86-runs/r1-miss-20261104/keypattern
PINS=(0:3500000 1:3500000 2:3500000 3:3500000 4:3500000 5:3500000)
L=$R/log-noprintk
mkdir "$L"
PLY=
finish()
{
    status=$?
    trap - EXIT
    for name in malloc funclatency callloop mallocloop; do sudo -n pkill -9 -x "$name" 2>/dev/null || true; done
    for b in dev-a4ac0d31 dev-7ad5a152; do sudo -n "$R/builds/$b/bpftimetool" remove >>"$L/cleanup.log" 2>&1 || true; done
    if [ -e "$L/pin-state.txt" ]; then "$K/pin-cpus.sh" restore "$L/pin-state.txt" || status=99; fi
    "$K/pin-cpus.sh" show 0 1 2 3 4 5 >"$L/pin-restored.txt" 2>&1 || true
    if [ -n "$PLY" ]; then sudo -n taskset -pc 0-15 "$PLY" >>"$L/plymouthd-affinity.txt" 2>&1 || true; fi
    printf '%s\n' "$status" >"$L/exit-status.txt"
    date --iso-8601=seconds >"$L/finished-at.txt"
    exit "$status"
}
trap finish EXIT
date --iso-8601=seconds >"$L/started-at.txt"
pgrep -af 'benchmark/test|uprobe/uprobe|keyloop|callloop|mallocloop|sslsniff|nginx|wrk|stress-ng' | grep -v -e pgrep -e "$R/" >"$L/leftover-processes.txt" || true
find /dev/shm -mindepth 1 -maxdepth 1 -user root >"$L/root-shm.txt"
if [ -s "$L/leftover-processes.txt" ] || [ -s "$L/root-shm.txt" ] || [ -e /dev/shm/bpftime_maps_shm ]; then
    echo "leftover processes or shared memory" >&2; exit 1
fi
PLY=$(pgrep -x plymouthd || true)
if [ -n "$PLY" ]; then taskset -pc "$PLY" >"$L/plymouthd-affinity.txt"; sudo -n taskset -pc 12-14 "$PLY" >>"$L/plymouthd-affinity.txt"; fi
"$K/pin-cpus.sh" pin "$L/pin-state.txt" "${PINS[@]}"
"$K/pin-cpus.sh" show 0 1 2 3 4 5 >"$L/pin-pinned.txt"
cd "$R/harness"
timeout 3600 python3 measure_mt_noprintk.py --tools malloc-example \
    --backends none,kernel,dev-7ad5a152 --rounds 3 --out "$R/measure-mt-noprintk/measure.jsonl" \
    >"$R/measure-mt-noprintk.log" 2>&1
