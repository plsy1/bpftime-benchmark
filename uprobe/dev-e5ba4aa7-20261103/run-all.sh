#!/bin/bash
# 11/03 after the daily job: hash map step 1 (42595149, shared count) and step 2 (e5ba4aa7,
# chains + per-CPU free lists) against kernel, a4ac0d31 (and a7d54d7e in the suite):
# key-pattern matrix, suite hash cases, structure harness, counters/churn/tracking table,
# then the complete ten-round uprobe suite. One stage at a time; each bounded.
set -u
N=/home/jetson/src/results/uprobe/dev-e5ba4aa7-20261103
U=/home/jetson/src/results/uprobe
S=$U/hash-redesign-survey-20261102/structure
K=$U/hash-keypattern-20261103
# Wait for the daily job (timer ~03:32, ~40 min) to be over, at most until 05:00.
while :; do
  now=$(date +%H%M)
  active=$(systemctl is-active bpftime-jetson-daily.service 2>/dev/null)
  if [ "$now" -ge 0412 ] && [ "$active" != active ] && [ "$active" != activating ]; then break; fi
  if [ "$now" -ge 0500 ]; then echo "daily job still running at 05:00, giving up"; exit 1; fi
  sleep 30
done
date; uname -r
python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
[ $? -eq 0 ] || exit 1
pgrep -af 'keyloop|trackloop|countloop|churnloop|benchmark.py' && { echo leftovers; exit 1; }
mkdir -p $N/keys $N/structure $N/gate
cd $K && timeout 6000 python3 run_keys.py --backends kernel,dev-a4ac0d31,dev-42595149,dev-e5ba4aa7 \
  --out $N/keys/keys.jsonl > $N/keys/keys.log 2>&1
echo "keys exit $?"; date
timeout 1800 sudo -n taskset -c 5 python3 $U/dev-6ab9cbfe-20261101/hash-bisect/run_hash.py \
  --builds a7d54d7e,dev-a4ac0d31,dev-42595149,dev-e5ba4aa7 --reps 5 --iters 50000 \
  --out $N/suite.jsonl > $N/suite.log 2>&1
echo "suite exit $?"; date
sudo -n chown -R jetson:jetson $N
for v in step1 mix; do
  for i in $(seq 0 11); do
    timeout 600 taskset -c 5 $S/structure_$v 100000 $i 2 >> $N/structure/$v.jsonl 2>> $N/structure/$v.stderr
    rm -f /dev/shm/bpftime_hash_structure_*
  done
done
for i in $(seq 0 11); do
  timeout 600 taskset -c 5 $S/structure2 100000 $i 2 >> $N/structure/s2.jsonl 2>> $N/structure/s2.stderr
  rm -f /dev/shm/bpftime_hash_structure_*
done
echo "structure done"; date
B=dev-42595149,dev-e5ba4aa7
timeout 3600 python3 $U/map-table-20261028/run_count.py --backends kernel,$B \
  --objs count-hash.bpf.o,count-percpu-hash.bpf.o,count-control.bpf.o \
  --threads 1,2,4,6 --nkeys 1,64 --reps 3 --out $N/gate/count.jsonl > $N/gate/count.log 2>&1
echo "count exit $?"; date
timeout 1800 python3 $U/map-threads-20261028/run_churn.py --backends $B --objs churn-k4-v8.bpf.o,control.bpf.o \
  --threads 1,6 --reps 3 --out $N/gate/churn.jsonl > $N/gate/churn.log 2>&1
echo "churn exit $?"; date
timeout 1800 python3 $U/map-table-20261028/run_track.py --backends $B --objs track.bpf.o,track-control.bpf.o \
  --threads 1,6 --live 1000,900000 --reps 3 --out $N/gate/track.jsonl > $N/gate/track.log 2>&1
echo "track exit $?"; date
sudo -n chown -R jetson:jetson $N
timeout 3600 bash $N/full10-e5ba4aa7/run-full.sh > $N/full10-e5ba4aa7/full-console.log 2>&1
echo "full10 exit $?"; date
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
