#!/usr/bin/env bash
# Step 2 (striped writers): churn, counters, tracking table; kernel vs c98951a4 (before) vs cb3f29d2 (after).
set -u
D=/home/jetson/src/results/uprobe/hash-striped-writers-20261030
U=/home/jetson/src/results/uprobe
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
python3 $U/map-threads-20261028/run_churn.py --backends kernel,c98951a4,9c3e0345,cb3f29d2 \
  --objs churn-k4-v8.bpf.o,control.bpf.o --threads 1,2,4,6 --reps 3 \
  --out $D/churn.jsonl > $D/churn.log 2>&1
python3 $U/map-table-20261028/run_count.py --backends kernel,c98951a4,cb3f29d2 \
  --objs count-hash.bpf.o,count-control.bpf.o --threads 1,2,4,6 --nkeys 1,64 --reps 3 \
  --out $D/count.jsonl > $D/count.log 2>&1
python3 $U/map-table-20261028/run_track.py --backends kernel,c98951a4,cb3f29d2 \
  --objs track.bpf.o,track-control.bpf.o --threads 1,2,4,6 --live 1000,900000 --reps 3 \
  --out $D/track.jsonl > $D/track.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
