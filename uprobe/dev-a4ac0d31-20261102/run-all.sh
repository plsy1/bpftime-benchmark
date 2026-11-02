#!/bin/bash
# 11/02 evening: final head a4ac0d31 (per-CPU hash without the map lock + flatten):
# gate (counters with the kernel, churn, tracking table, suite cases) against 68f19c1f, then
# the ssl-nginx fair round and the complete ten-round uprobe suite, one after another.
set -u
N=/home/jetson/src/results/uprobe/dev-a4ac0d31-20261102
G=/home/jetson/src/results/uprobe/percpu-hash-lockfree-20261102/final
U=/home/jetson/src/results/uprobe
python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
[ $? -eq 0 ] || exit 1
date; uname -r
mkdir -p $G
B=dev-68f19c1f,dev-a4ac0d31
timeout 3600 python3 $U/map-table-20261028/run_count.py --backends kernel,$B \
  --objs count-hash.bpf.o,count-percpu-hash.bpf.o,count-control.bpf.o \
  --threads 1,2,4,6 --nkeys 1,64 --reps 3 --out $G/count.jsonl > $G/count.log 2>&1
echo "count exit $?"; date
timeout 1800 python3 $U/map-threads-20261028/run_churn.py --backends $B --objs churn-k4-v8.bpf.o,control.bpf.o \
  --threads 1,6 --reps 3 --out $G/churn.jsonl > $G/churn.log 2>&1
echo "churn exit $?"; date
timeout 1800 python3 $U/map-table-20261028/run_track.py --backends $B --objs track.bpf.o,track-control.bpf.o \
  --threads 1,6 --live 1000,900000 --reps 3 --out $G/track.jsonl > $G/track.log 2>&1
echo "track exit $?"; date
timeout 1800 sudo -n taskset -c 5 python3 $U/dev-6ab9cbfe-20261101/hash-bisect/run_hash.py \
  --builds dev-68f19c1f,a7d54d7e,dev-a4ac0d31 --reps 5 --iters 50000 --out $G/suite.jsonl > $G/suite.log 2>&1
echo "suite exit $?"; date
sudo -n chown -R jetson:jetson $G
timeout 4800 bash $N/ssl-nginx/run.sh > $N/ssl-nginx/run.console.log 2>&1
echo "ssl-nginx exit $?"; date
sudo -n docker rm -f ssl-a4ac >/dev/null 2>&1
timeout 3600 bash $N/full10-a4ac0d31/run-full.sh > $N/full10-a4ac0d31/full-console.log 2>&1
echo "full10 exit $?"; date
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
