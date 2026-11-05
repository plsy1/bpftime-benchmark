#!/usr/bin/env bash
# Bursts at matched arrival rates, with delivery delays (see README, finding 5): kernel bpf vs
# fix-final. Calls 4 us apart by the clock (above both probes' cost), bursts every 10 ms.
set -u
D=/home/jetson/src/results/uprobe/output-dims-20261106
cd $D
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > lat-clock-before.txt
date +%T > lat-started
B4=out-perf-256.bpf.o,out-perf-4096.bpf.o,out-rbres-256.bpf.o,out-rbres-4096.bpf.o
# L1: matched arrival, bursts of 10/100/1000, a consumer doing nothing or 2 us per record.
for burst in 10 100 1000; do
  for cons in 0 2000; do
    timeout 900 python3 run.py --objs $B4 --threads 1,4 --reps 2 --secs 4 --workload burst --burst $burst --pause-us 10000 --period-ns 4000 --consumer-ns $cons --latency --out l1-matched.jsonl >> l1.log 2>&1
  done
done
# L2: M2's back-to-back bursts of 100 again, now with the delays.
timeout 900 python3 run.py --objs $B4 --threads 1,4 --reps 2 --secs 4 --workload burst --burst 100 --pause-us 10000 --consumer-ns 2000 --latency --out l2-backtoback.jsonl > l2.log 2>&1
date +%T > lat-finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > lat-clock-after.txt
