#!/bin/bash
# 11/04: memleak deadlock fix (frozen build $1, e.g. dev-XXXXXXXX) against bpf and dev-7ad5a152.
# 1. audit memleak/funclatency, single thread (memleak count vs bpf: agent work stays invisible);
# 2. memleak 1/2/4/6 threads (12/12 hangs before), funclatency-libbpf 1/2/4/6 threads (no cost
#    change expected: the hot path is untouched), 3 rounds x 10 s; 3. SIGUSR1 detach on 6 threads.
# Each stage bounded; cpu5 1728000 and no leftovers before.
set -u
B=$1
N=/home/jetson/src/results/uprobe/memleak-fix-20261104
H=/home/jetson/src/realprog-20261026-btf/harness
date; uname -r
check() {
  python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
  [ $? -eq 0 ] || exit 1
  pgrep -af 'keyloop|trackloop|countloop|churnloop|benchmark.py|callloop|mallocloop|nginx|wrk' | grep -v pgrep && { echo leftovers; exit 1; }
  ls -la /dev/shm | awk '$3=="root"' | grep -v -e nvsciipc_init -e ' \.$' -e ' \.\.$'
}
check
mkdir -p $N/audit $N/measure-mt
cd $H && timeout 900 python3 audit.py --backends kernel,$B --tools memleak,funclatency-libbpf --secs 4 --out $N/audit/audit.jsonl > $N/audit/audit.log 2>&1
echo "audit exit $?"; date
check
cd $H && timeout 1800 python3 measure_mt.py --tools memleak --backends kernel,$B --out $N/measure-mt/memleak.jsonl > $N/measure-mt/memleak.log 2>&1
echo "memleak exit $?"; date
check
cd $H && timeout 2400 python3 measure_mt.py --tools funclatency-libbpf --backends kernel,dev-7ad5a152,$B --out $N/measure-mt/funclatency.jsonl > $N/measure-mt/funclatency.log 2>&1
echo "funclatency exit $?"; date
check
timeout 600 python3 $N/detach_mt.py $B $N/detach.jsonl > $N/detach.log 2>&1
echo "detach exit $?"; date
sudo -n chown -R jetson:jetson $N
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
