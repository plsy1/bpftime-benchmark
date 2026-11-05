#!/usr/bin/env bash
# Diagnosis: fix-rbwake2 against its agent with the notify fencing and loading waiters but never waking.
# fix-rbwake (ce06c39c + ringbuf submit wakes a caught-up consumer), with output-ab's
# harness (reviews-20261031/refixes2/output-ab: outattach + countloop, consumer in
# ring_buffer__poll(rb, 100)).
set -u
D=/home/jetson/src/results/reviews-20261105/fixes/output-ab
H=/home/jetson/src/results/reviews-20261031/refixes2/output-ab
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before3.txt
timeout 3600 python3 $H/run_out.py --backends dev-b60b99c3,fix-rbwake2,exp-rbwake-nowake --objs out-ringbuf.bpf.o \
  --threads 1,6 --nkeys 1 --reps 5 --secs 6 --out $D/out3.jsonl > $D/out3.log 2>&1
echo rc=$?
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after3.txt
