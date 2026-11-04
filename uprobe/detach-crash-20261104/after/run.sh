#!/bin/bash
# 11/04: problem 7 after the fix (frozen build $1). 1. SIGUSR1 detach, tool and workload started
# apart (detach_manual.sh): funclatency 1/2/6 threads, memleak 1/6 threads, 3 rounds each; the
# workload must exit 0 and print its ops. 2. memleak -c through the harness, SIGUSR1 after 4 s
# (memleak-fix-20261104/detach_mt.py), 1 and 6 threads. 3. attached cost: funclatency-libbpf,
# memleak, bpftrace-uretprobe-hist at 1/2/4/6 threads against bpf and dev-efe08cbd.
# cpu5 1728000 and no leftovers before each stage.
set -u
B=$1
N=/home/jetson/src/results/uprobe/detach-crash-20261104/after
H=/home/jetson/src/realprog-20261026-btf/harness
date; uname -r
check() {
  python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
  [ $? -eq 0 ] || exit 1
  pgrep -af 'keyloop|trackloop|countloop|churnloop|benchmark.py|callloop|mallocloop|nginx|wrk|memleak|funclatency' | grep -v -e pgrep -e run.sh -e detach_manual && { echo leftovers; exit 1; }
  ls -la /dev/shm | awk '$3=="root"' | grep -v -e nvsciipc_init -e ' \.$' -e ' \.\.$'
}
check
for r in 0 1 2; do
  for t in 1 2 6; do timeout 120 $N/detach_manual.sh $B funclatency $t $r; done
  for t in 1 6; do timeout 120 $N/detach_manual.sh $B memleak $t $r; done
done
echo "manual done"; date
check
for t in t1 t6; do
  timeout 600 python3 /home/jetson/src/results/uprobe/memleak-fix-20261104/detach_mt.py $B $N/detach-harness.jsonl /home/jetson/src/realprog-20261026-btf/workloads-mt/$t > $N/detach-harness.$t.log 2>&1
  echo "harness $t exit $?"
done
date
check
mkdir -p $N/measure-mt
cd $H && timeout 3600 python3 measure_mt.py --tools funclatency-libbpf,memleak,bpftrace-uretprobe-hist --backends kernel,dev-efe08cbd,$B --out $N/measure-mt/measure.jsonl > $N/measure-mt/measure.log 2>&1
echo "measure exit $?"; date
check
sudo -n chown -R jetson:jetson $N
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
