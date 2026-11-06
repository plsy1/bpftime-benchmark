#!/usr/bin/env bash
# Round 3 (more-dims-20261106 list): R1 opensnoop's record size against one that divides the
# ring; R2 perf lost counts flushed by a later sample from the same thread; R3 pauses between
# bursts from 0.5 ms to 1 s. kernel bpf vs fix-final.
set -u
D=/home/jetson/src/results/uprobe/output-dims-20261106
cd $D
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > r3-clock-before.txt
date +%T > r3-started
# R1: 8,240 B (8,248 with the header: a padding every 31 records) vs 8,184 B (8,192: divides).
for burst in 50 100; do
  for cons in 0 2000; do
    timeout 900 python3 run.py --objs out-rbres-8240.bpf.o,out-rbres-8184.bpf.o --threads 1,4 --reps 2 --secs 4 --workload burst --burst $burst --pause-us 10000 --period-ns 4000 --consumer-ns $cons --latency --out r1-opensnoop-size.jsonl >> r1.log 2>&1
  done
done
# R2: lossy perf bursts, each thread makes 3 more calls 50 ms after the run.
timeout 900 python3 run.py --objs out-perf-256.bpf.o,out-perf-4096.bpf.o --threads 1,4 --reps 3 --secs 4 --workload burst --burst 1000 --pause-us 1000 --consumer-ns 2000 --flush --out r2-lost-flush.jsonl > r2.log 2>&1
# R3: bursts of 100 at matched arrival, one thread, pauses 0.5 ms, 10 ms, 100 ms, 1 s.
O3=out-rbres-4096.bpf.o,out-rbres-4088.bpf.o,out-perf-256.bpf.o
timeout 900 python3 run.py --objs $O3 --threads 1 --reps 2 --secs 4 --workload burst --burst 100 --pause-us 500 --period-ns 4000 --latency --first-gap-ns 250000 --out r3-pause.jsonl >> r3.log 2>&1
timeout 900 python3 run.py --objs $O3 --threads 1 --reps 2 --secs 4 --workload burst --burst 100 --pause-us 10000 --period-ns 4000 --latency --first-gap-ns 5000000 --out r3-pause.jsonl >> r3.log 2>&1
timeout 900 python3 run.py --objs $O3 --threads 1 --reps 2 --secs 8 --workload burst --burst 100 --pause-us 100000 --period-ns 4000 --latency --first-gap-ns 50000000 --out r3-pause.jsonl >> r3.log 2>&1
timeout 1500 python3 run.py --objs $O3 --threads 1 --reps 2 --secs 30 --workload burst --burst 100 --pause-us 1000000 --period-ns 4000 --latency --first-gap-ns 500000000 --out r3-pause.jsonl >> r3.log 2>&1
date +%T > r3-finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > r3-clock-after.txt
