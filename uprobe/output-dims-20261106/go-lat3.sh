#!/usr/bin/env bash
# L3: ring buffer records that divide the ring (248 + 8, 4088 + 8 bytes: no padding record at
# its end) against 256 and 4096, matched arrival as L1.
set -u
D=/home/jetson/src/results/uprobe/output-dims-20261106
cd $D
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > lat3-clock-before.txt
date +%T > lat3-started
R4=out-rbres-248.bpf.o,out-rbres-256.bpf.o,out-rbres-4088.bpf.o,out-rbres-4096.bpf.o
for burst in 100 1000; do
  for cons in 0 2000; do
    timeout 900 python3 run.py --objs $R4 --threads 1,4 --reps 2 --secs 4 --workload burst --burst $burst --pause-us 10000 --period-ns 4000 --consumer-ns $cons --latency --out l3-nopad.jsonl >> l3.log 2>&1
  done
done
date +%T > lat3-finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > lat3-clock-after.txt
