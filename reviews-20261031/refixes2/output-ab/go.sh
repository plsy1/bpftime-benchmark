#!/usr/bin/env bash
# The third review's fixes (dev f73a45f1) against 9aeb2863's code (opt-identity2):
# perf output, ringbuf output, control (pid_tgid only); 1 and 6 threads.
set -u
D=/home/jetson/src/results/reviews-20261031/refixes2/output-ab
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
python3 $D/run_out.py --backends opt-identity2,dev-f73a45f1 \
  --threads 1,6 --nkeys 1 --reps 5 --secs 6 --out $D/out.jsonl > $D/out.log 2>&1
echo rc=$?
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
