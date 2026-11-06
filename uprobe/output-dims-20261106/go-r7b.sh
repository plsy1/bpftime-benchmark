#!/usr/bin/env bash
# R7b: more producer processes than CPUs (6, 16, 64), with the per-call durations (worst and
# median process) and the control, to see a user-space lock held by a descheduled process.
set -u
D=/home/jetson/src/results/uprobe/output-dims-20261106
cd $D
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > r7b-clock-before.txt
date +%T > r7b-started
for procs in 6 16 64; do
  timeout 1500 python3 run.py --objs out-control.bpf.o,out-perf-256.bpf.o,out-rbres-256.bpf.o --threads 1 --procs $procs --reps 2 --secs 6 --workload burst --burst 10 --pause-us 10000 --period-ns 4000 --latency --call-hist --out r7b-oversub.jsonl >> r7b.log 2>&1
done
date +%T > r7b-finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > r7b-clock-after.txt
