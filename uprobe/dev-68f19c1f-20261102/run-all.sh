#!/bin/bash
# 11/02: phase 1 re-check on 68f19c1f (suite cases, per-CPU counters), then the
# ssl-nginx fair round and the complete ten-round uprobe suite, one after another.
set -u
N=/home/jetson/src/results/uprobe/dev-68f19c1f-20261102
G=/home/jetson/src/results/uprobe/hash-single-thread-20261102/gate2
U=/home/jetson/src/results/uprobe
python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
[ $? -eq 0 ] || exit 1
date; uname -r
timeout 1800 sudo -n taskset -c 5 python3 $U/dev-6ab9cbfe-20261101/hash-bisect/run_hash.py \
  --builds dev-6ab9cbfe,a7d54d7e,dev-68f19c1f --reps 3 --iters 50000 --out $G/suite.jsonl > $G/suite.log 2>&1
echo "suite exit $?"; date
timeout 1800 python3 $U/map-table-20261028/run_count.py --backends dev-6ab9cbfe,dev-68f19c1f \
  --objs count-percpu-hash.bpf.o,count-control.bpf.o --threads 1,6 --nkeys 1,64 --reps 3 \
  --out $G/count.jsonl > $G/count.log 2>&1
echo "count exit $?"; date
sudo -n chown -R jetson:jetson $G
timeout 4800 bash $N/ssl-nginx/run.sh > $N/ssl-nginx/run.console.log 2>&1
echo "ssl-nginx exit $?"; date
sudo -n docker rm -f ssl-68f1 >/dev/null 2>&1
timeout 3600 bash $N/full10-68f19c1f/run-full.sh > $N/full10-68f19c1f/full-console.log 2>&1
echo "full10 exit $?"; date
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
