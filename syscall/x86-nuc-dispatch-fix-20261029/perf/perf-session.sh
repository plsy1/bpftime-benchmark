#!/usr/bin/env bash
# x86 NUC: profile.sh for rewrite (after), agent-other and agent-exit before (dev
# 7a7d29b8, build-7a7d29b8) and after (2e850c45, build-2e850c45) the dispatch fix.
# CPU 2 pinned at 1.7 GHz, turbo off (freq.sh from ../session), every leg under
# taskset -c 2, vm.mmap_min_addr = 0 during the session; both restored on exit.
# perf.data files are moved to /home/y/src/bpftime-x86-runs/dispatch-fix-perf (not committed).
set -euo pipefail
D=$(cd "$(dirname "$0")" && pwd)
F=$D/../session
SRCTREE=/home/y/src/bpftime-wt-uprobe-177a1dc
KEEP=/home/y/src/bpftime-x86-runs/dispatch-fix-perf
CPU=2 KHZ=1700000
TSC=$(sed -n 's/^tsc_mhz=//p' "$F/machine.txt")
mkdir "$D/before" "$D/after" "$KEEP"
STATE=$D/freq-before.txt
MMAP_ORIG=$(cat /proc/sys/vm/mmap_min_addr)
finish()
{
    status=$?
    trap - EXIT
    echo "$MMAP_ORIG" | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null || status=98
    echo "vm.mmap_min_addr original $MMAP_ORIG, after restore $(cat /proc/sys/vm/mmap_min_addr)" >"$D/mmap_min_addr.txt"
    if [ -e "$STATE" ]; then "$F/freq.sh" restore "$CPU" "$STATE" || status=99; fi
    "$F/freq.sh" show "$CPU" >"$D/freq-restored.txt" 2>&1 || true
    printf '%s\n' "$status" >"$D/exit-status.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
test ! -e /dev/shm/bpftime_maps_shm
"$F/freq.sh" pin "$CPU" "$KHZ" "$STATE" turbo-off
sudo -n modprobe msr
for try in $(seq 15); do
    sudo -n python3 "$F/busy-check.py" "$TSC" "$CPU:$KHZ" >>"$D/freq-pinned.txt" && break
    test "$try" -lt 15
done
echo 0 | sudo -n tee /proc/sys/vm/mmap_min_addr >/dev/null
run() {  # <before|after> <build> <leg>
    sudo -n python3 "$F/busy-check.py" "$TSC" "$CPU:$KHZ" >"$D/$1/$3.freq-before.txt"
    OUT=$D/$1 BPFTIME_BUILD=$SRCTREE/$2 taskset -c "$CPU" "$D/profile.sh" "$3" >"$D/$1/$3.console.txt" 2>&1
    mv "$D/$1/$3.perf.data" "$KEEP/$1-$3.perf.data"
}
run after build-2e850c45 rewrite
run before build-7a7d29b8 agent-other
run after build-2e850c45 agent-other
run before build-7a7d29b8 agent-exit
run after build-2e850c45 agent-exit
sudo -n python3 "$F/busy-check.py" "$TSC" "$CPU:$KHZ" >"$D/freq-after.txt"
