#!/usr/bin/env bash
# The output-dimension matrix (see README): kernel bpf vs fix-final (dev d46ceb8f).
set -u
D=/home/jetson/src/results/uprobe/output-dims-20261106
cd $D
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
date +%T > started
ALL=out-control.bpf.o,out-perf-8.bpf.o,out-perf-256.bpf.o,out-perf-4096.bpf.o,out-rbout-8.bpf.o,out-rbout-256.bpf.o,out-rbout-4096.bpf.o,out-rbres-8.bpf.o,out-rbres-256.bpf.o,out-rbres-4096.bpf.o
# M1: steady, as fast as the probe goes, a consumer doing nothing per record.
timeout 3600 python3 run.py --objs $ALL --threads 1,2,4,6 --reps 3 --secs 6 --out m1-steady.jsonl > m1.log 2>&1
# M2: bursts of 10/100/1000 every 10 ms, back to back or 2 us apart, a consumer spending 2 us per record.
B4=out-perf-4096.bpf.o,out-rbres-4096.bpf.o,out-perf-256.bpf.o,out-rbres-256.bpf.o
for burst in 10 100 1000; do
  for gap in 0 2000; do
    timeout 1800 python3 run.py --objs $B4 --threads 1,4 --reps 2 --secs 4 --workload burst --burst $burst --pause-us 10000 --gap-ns $gap --consumer-ns 2000 --out m2-burst.jsonl >> m2.log 2>&1
  done
done
# M3: steady with that consumer: sustained overload.
timeout 1200 python3 run.py --objs out-perf-256.bpf.o,out-rbres-256.bpf.o --threads 1,4 --reps 2 --secs 6 --consumer-ns 2000 --out m3-overload.jsonl > m3.log 2>&1
date +%T > finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
