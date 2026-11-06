#!/usr/bin/env bash
# Round 4 (more-dims-20261106 list): R4 the traced call's own duration (tail) in matched bursts;
# R5 variable record sizes; R6 producers in separate processes against threads of one process,
# paced and back to back; R7 16 and 64 producer processes at a low rate.
# kernel bpf vs fix-final (R4 also untraced).
set -u
D=/home/jetson/src/results/uprobe/output-dims-20261106
cd $D
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > r4-clock-before.txt
date +%T > r4-started
O4=out-control.bpf.o,out-perf-256.bpf.o,out-perf-4096.bpf.o,out-rbres-256.bpf.o,out-rbres-4096.bpf.o,out-rbres-4088.bpf.o
timeout 1200 python3 run.py --backends none,kernel,fix-final --objs $O4 --threads 1,4 --reps 2 --secs 4 --workload burst --burst 100 --pause-us 10000 --period-ns 4000 --call-hist --out r4-call-tail.jsonl > r4.log 2>&1
for burst in 100 1000; do
  timeout 900 python3 run.py --objs out-perf-var.bpf.o,out-rbout-var.bpf.o,out-rbres-var.bpf.o --threads 1,4 --reps 2 --secs 4 --workload burst --burst $burst --pause-us 10000 --period-ns 4000 --latency --out r5-varsize.jsonl >> r5.log 2>&1
done
O6=out-perf-256.bpf.o,out-rbres-256.bpf.o,out-rbres-4088.bpf.o
timeout 900 python3 run.py --objs $O6 --threads 1,2,4 --reps 2 --secs 4 --workload burst --burst 100 --pause-us 10000 --period-ns 4000 --latency --out r6-procs-paced.jsonl >> r6.log 2>&1
for procs in 2 4; do
  timeout 900 python3 run.py --objs $O6 --threads 1 --procs $procs --reps 2 --secs 4 --workload burst --burst 100 --pause-us 10000 --period-ns 4000 --latency --out r6-procs-paced.jsonl >> r6.log 2>&1
done
O6B=out-control.bpf.o,out-perf-8.bpf.o,out-rbres-8.bpf.o
timeout 900 python3 run.py --objs $O6B --threads 4 --reps 3 --secs 4 --workload burst --burst 10000 --pause-us 0 --out r6-procs-steady.jsonl >> r6.log 2>&1
timeout 900 python3 run.py --objs $O6B --threads 1 --procs 4 --reps 3 --secs 4 --workload burst --burst 10000 --pause-us 0 --out r6-procs-steady.jsonl >> r6.log 2>&1
for procs in 16 64; do
  timeout 1200 python3 run.py --objs out-perf-256.bpf.o,out-rbres-256.bpf.o --threads 1 --procs $procs --reps 2 --secs 6 --workload burst --burst 10 --pause-us 10000 --period-ns 4000 --latency --out r7-many-procs.jsonl >> r7.log 2>&1
done
date +%T > r4-finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > r4-clock-after.txt
