#!/bin/bash
# 11/04: real tools at 1/2/4/6 threads, before the multi-thread map work (exp-c98951a4-gumfix:
# c98951a4 + the ff5515d8 deadlock fix, so memleak runs) against now (dev-ff5515d8) and bpf.
# measure_mt.py's 5 tools, 3 rounds x 10 s, backends rotated. cpu5 1728000 and no leftovers before.
set -u
N=/home/jetson/src/results/uprobe/vs-pre-mt-20261104
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
mkdir -p $N/measure-mt
cd $H && timeout 5400 python3 measure_mt.py --backends kernel,exp-c98951a4-gumfix,dev-ff5515d8 --out $N/measure-mt/measure.jsonl > $N/measure-mt/measure.log 2>&1
echo "measure-mt exit $?"; date
check
sudo -n chown -R jetson:jetson $N
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
