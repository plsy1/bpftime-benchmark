#!/usr/bin/env bash
# improve-20261106 item 4: the ring buffer reserve lock, step by step (all with probe checks on):
# f5b388cd (before), 2a0ed6ff (4a inline lock + 4c one handler read), d08ac155 (4b lock line),
# 8a87aaed (4d positions on the lock's line), b6da99bf (4e yield), and the kernel. output-dims
# run.py: R6 back to back (1 thread; 4 threads of one process; 4 single-thread processes),
# R7b (64 single-thread processes on 6 CPUs, per-call durations).
set -u
R=/home/jetson/src/results/uprobe/improve-20261106/n4-reserve-lock
OD=/home/jetson/src/results/uprobe/output-dims-20261106
cd $OD
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-before.txt
date +%T > $R/started
BK=kernel,dev-f5b388cd-probecheck,dev-2a0ed6ff-probecheck,dev-d08ac155-probecheck,dev-8a87aaed-probecheck,dev-b6da99bf-probecheck
timeout 1800 python3 run.py --backends $BK --objs out-control.bpf.o,out-rbres-8.bpf.o --threads 1,4 --reps 3 --secs 4 --workload burst --burst 10000 --pause-us 0 --out $R/steady.jsonl > $R/steady.log 2>&1
timeout 1800 python3 run.py --backends $BK --objs out-control.bpf.o,out-rbres-8.bpf.o --threads 1 --procs 4 --reps 3 --secs 4 --workload burst --burst 10000 --pause-us 0 --out $R/steady.jsonl >> $R/steady.log 2>&1
timeout 2400 python3 run.py --backends $BK --objs out-rbres-256.bpf.o --threads 1 --procs 64 --reps 2 --secs 6 --workload burst --burst 10 --pause-us 10000 --period-ns 4000 --latency --call-hist --out $R/oversub.jsonl > $R/oversub.log 2>&1
date +%T > $R/finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-after.txt
echo done
