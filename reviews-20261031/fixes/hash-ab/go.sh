#!/usr/bin/env bash
# Hash map robustness (F) gate: f4fdcbcd (last measured), dev b9c849fb
# (review fixes, before F), hash-fmix (F). Single thread and 6 threads must
# not get slower with F.
set -u
D=/home/jetson/src/results/reviews-20261031/fixes/hash-ab
U=/home/jetson/src/results/uprobe
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
python3 $U/map-threads-20261028/run_churn.py --backends f4fdcbcd,dev-b9c849fb,hash-fmix \
  --objs churn-k4-v8.bpf.o,control.bpf.o --threads 1,2,4,6 --reps 3 \
  --out $D/churn.jsonl > $D/churn.log 2>&1
python3 $U/map-table-20261028/run_track.py --backends f4fdcbcd,dev-b9c849fb,hash-fmix \
  --objs track.bpf.o,track-control.bpf.o --threads 1,6 --live 1000,900000 --reps 3 \
  --out $D/track.jsonl > $D/track.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
