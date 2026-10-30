#!/usr/bin/env bash
# Map program paths before (f4fdcbcd) and after the review fixes (8c108309):
# per-CPU array and hash (current-CPU checks, slots for every key size),
# hash (aligned values; k4/v8 layout unchanged), control.
set -u
D=/home/jetson/src/results/reviews-20261031/fixes/counters-ab
U=/home/jetson/src/results/uprobe
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
python3 $U/map-table-20261028/run_count.py --backends f4fdcbcd,dev-8c108309 \
  --objs count-percpu-array.bpf.o,count-percpu-hash.bpf.o,count-hash.bpf.o,count-control.bpf.o \
  --threads 1,6 --nkeys 1,64 --reps 3 --out $D/count.jsonl > $D/count.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
