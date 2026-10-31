#!/usr/bin/env bash
# Output paths before (cpucheck-unsigned: f2d6ae7d's tree) and after the
# second review's fixes (dev-7553abb8): perf output (shard search by
# namespace, identity epoch), ringbuf output (reservation owners), control
# (bpf_get_current_pid_tgid through the shared identity cache).
set -u
D=/home/jetson/src/results/reviews-20261031/refixes/output-ab
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
python3 $D/run_out.py --backends cpucheck-unsigned,dev-7553abb8 \
  --threads 1,6 --nkeys 1 --reps 5 --secs 6 --out $D/out.jsonl > $D/out.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
