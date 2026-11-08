#!/usr/bin/env bash
# startup-20261108: the AOT cache (dev a90d8d0b..NEW) against the dev before it and bpf.
#   old = dev-9c088441-probecheck (dev 9c088441; 8a0afd36 adds a test only), checks on;
#   new = $1 (frozen build of the cache commits), checks on; new:noaot = new with
#   BPFTIME_AOT_CACHE=0 in the tool (agents compile, as before).
#  1. memleak -c: the traced process's start and the tool's set-up (10 runs each).
#  2. K processes starting at once, one program (1/2/4/8/16, 20 rounds, 2 passes).
#  3. steady state, new against old: output cost per call (1 and 6 threads, 3 x 6 s) and the
#     real tools on 1/2/4/6 threads (measure_mt.py): AOT code must not be slower than JIT code.
set -u
NEW=${1:?usage: go.sh NEW_BUILD}
OLD=dev-9c088441-probecheck
N=/home/jetson/src/results/uprobe/startup-20261108
O=/home/jetson/src/results/uprobe/output-dims-20261106
H=/home/jetson/src/realprog-20261026-btf/harness
L=$N/go.log
step() { echo "$(date +%T) $1" >> $L; }
check() {
  python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
  [ $? -eq 0 ] || { step "clock check failed"; exit 1; }
  # Process names, not command lines (a shell running this script names memleak-start).
  if pgrep -a '^(keyloop|trackloop|countloop|churnloop|burstloop|outattach|callloop|mallocloop|shortlived|spawnburst|memleak|nginx|wrk)$'; then step leftovers; exit 1; fi
}
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $N/clock-before.txt
step "start new=$NEW old=$OLD"
check
step memleak-start
timeout 1800 $N/memleak-start/run.sh $NEW $OLD 10 > $N/memleak-start/results.txt 2>&1
step "memleak-start exit $?"
check
step concurrent-start
cd $N/concurrent-start && timeout 3600 python3 run.py --backends kernel,$OLD,$NEW,$NEW:noaot --out $N/concurrent-start/burst.jsonl > $N/concurrent-start/burst.log 2>&1
step "concurrent-start exit $?"
check
step output-threads
cd $O && timeout 3000 python3 run.py --backends $OLD,$NEW --objs out-control.bpf.o,out-perf-8.bpf.o,out-rbres-8.bpf.o --threads 1,6 --reps 3 --secs 6 --out $N/output-threads.jsonl > $N/output-threads.log 2>&1
step "output-threads exit $?"
check
step realtools-mt
cd $H && timeout 5400 python3 measure_mt.py --backends $OLD,$NEW --out $N/realtools-mt.jsonl > $N/realtools-mt.log 2>&1
step "realtools-mt exit $?"
check
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $N/clock-after.txt
sudo -n chown -R jetson:jetson $N
step done
