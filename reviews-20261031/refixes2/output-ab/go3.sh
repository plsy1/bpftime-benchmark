#!/usr/bin/env bash
# ringbuf only: 9aeb2863 (opt-identity2), f73a45f1, and a busy head probed only when seen on two polls.
set -u
D=/home/jetson/src/results/reviews-20261031/refixes2/output-ab
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before3.txt
python3 $D/run_out.py --backends opt-identity2,dev-f73a45f1,dev-busyseen --objs out-ringbuf.bpf.o \
  --threads 1,6 --nkeys 1 --reps 5 --secs 6 --out $D/out3.jsonl > $D/out3.log 2>&1
echo rc=$?
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after3.txt
