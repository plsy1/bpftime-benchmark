#!/usr/bin/env bash
# remeasure-20261108: the slow items not measured on the current dev, against bpf.
# dev-9c088441-probecheck = dev 9c088441 (8a0afd36 adds a test only) with probe checks on.
#  1. output cost per call at 1/2/4/6 threads (output-dims M1's settings: steady, 3 x 6 s);
#     the 11-07 final batch measured 1 and 4 threads only.
#  2. real tools on 1/2/4/6 threads (measure_mt.py, 3 rounds x 10 s); last run 11-04.
set -u
N=/home/jetson/src/results/uprobe/remeasure-20261108
O=/home/jetson/src/results/uprobe/output-dims-20261106
H=/home/jetson/src/realprog-20261026-btf/harness
B=kernel,dev-9c088441-probecheck
L=$N/go.log
step() { echo "$(date +%T) $1" >> $L; }
check() {
  python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
  [ $? -eq 0 ] || { step "clock check failed"; exit 1; }
  if pgrep -af 'keyloop|trackloop|countloop|churnloop|burstloop|outattach|callloop|mallocloop|shortlived|nginx|wrk' | grep -v pgrep; then step leftovers; exit 1; fi
}
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $N/clock-before.txt
check
step output-threads
cd $O && timeout 3000 python3 run.py --backends $B --objs out-control.bpf.o,out-perf-8.bpf.o,out-rbout-8.bpf.o,out-rbres-8.bpf.o --threads 1,2,4,6 --reps 3 --secs 6 --out $N/output-threads.jsonl > $N/output-threads.log 2>&1
step "output-threads exit $?"
check
step realtools-mt
cd $H && timeout 5400 python3 measure_mt.py --backends $B --out $N/realtools-mt.jsonl > $N/realtools-mt.log 2>&1
step "realtools-mt exit $?"
check
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $N/clock-after.txt
sudo -n chown -R jetson:jetson $N
step done
