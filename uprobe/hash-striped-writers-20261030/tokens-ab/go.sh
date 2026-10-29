#!/usr/bin/env bash
# Single-thread regression check: per-CPU token slots (cb3f29d2) vs tokens per stripe.
set -u
D=/home/jetson/src/results/uprobe/hash-striped-writers-20261030/tokens-ab
U=/home/jetson/src/results/uprobe
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
python3 $U/map-threads-20261028/run_churn.py --backends c98951a4,cb3f29d2,stripe-tokens \
  --objs churn-k4-v8.bpf.o,control.bpf.o --threads 1,2,4,6 --reps 3 \
  --out $D/churn.jsonl > $D/churn.log 2>&1
python3 $U/map-table-20261028/run_track.py --backends c98951a4,cb3f29d2,stripe-tokens \
  --objs track.bpf.o,track-control.bpf.o --threads 1,6 --live 1000 --reps 3 \
  --out $D/track.jsonl > $D/track.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
