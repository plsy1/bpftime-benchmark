#!/usr/bin/env bash
# Ring buffer wakeup (problem 13) A/B: dev-b60b99c3 (the code of 94f9ba8f) against
# fix-rbwake (ce06c39c + ringbuf submit wakes a caught-up consumer), with output-ab's
# harness (reviews-20261031/refixes2/output-ab: outattach + countloop, consumer in
# ring_buffer__poll(rb, 100)).
set -u
D=/home/jetson/src/results/reviews-20261105/fixes/output-ab
H=/home/jetson/src/results/reviews-20261031/refixes2/output-ab
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
timeout 3600 python3 $H/run_out.py --backends dev-b60b99c3,fix-rbwake --objs out-ringbuf.bpf.o,out-perf.bpf.o,out-control.bpf.o \
  --threads 1,6 --nkeys 1 --reps 5 --secs 6 --out $D/out.jsonl > $D/out.log 2>&1
echo rc=$?
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
