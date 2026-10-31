#!/usr/bin/env bash
# Round 2: opt-identity (inline identity fast path, ringbuf owner slot from
# the tid) against the same two builds.
set -u
D=/home/jetson/src/results/reviews-20261031/refixes/output-ab
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before2.txt
python3 $D/run_out.py --backends cpucheck-unsigned,dev-7553abb8,opt-identity \
  --threads 1,6 --nkeys 1 --reps 5 --secs 6 --out $D/out2.jsonl > $D/out2.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after2.txt
echo done > $D/finished2
