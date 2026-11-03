#!/bin/bash
# 11/03 evening: key-pattern matrix (kernel, e5ba4aa7, 518a6b99), then the complete ten-round
# uprobe suite on 518a6b99. One stage at a time; each bounded.
set -u
N=/home/jetson/src/results/uprobe/dev-518a6b99-20261103
U=/home/jetson/src/results/uprobe
K=$U/hash-keypattern-20261103
date; uname -r
python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
[ $? -eq 0 ] || exit 1
pgrep -af 'keyloop|trackloop|countloop|churnloop|benchmark.py' && { echo leftovers; exit 1; }
mkdir -p $N/keys
cd $K && timeout 4800 python3 run_keys.py --backends kernel,dev-e5ba4aa7,dev-518a6b99 \
  --out $N/keys/keys.jsonl > $N/keys/keys.log 2>&1
echo "keys exit $?"; date
sudo -n chown -R jetson:jetson $N
timeout 3600 bash $N/full10-518a6b99/run-full.sh > $N/full10-518a6b99/full-console.log 2>&1
echo "full10 exit $?"; date
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
sudo -n chown -R jetson:jetson $N
