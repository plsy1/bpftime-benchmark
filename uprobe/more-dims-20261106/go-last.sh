#!/usr/bin/env bash
# n3 again with the CO-RE sslsniff, the n5 cell with 600 perf-output threads, then the 2 h soak.
set -u
M=/home/jetson/src/results/uprobe/more-dims-20261106
L=$M/go-last.log
step() { echo "$(date +%T) $*" >> $L; }
step n3-tail
timeout 2700 $M/n3-tail/go.sh
step n5-600-threads
cd $M/n5-footprint
timeout 600 python3 run.py --objs out-perf-256.bpf.o --threads 600 --hold 5 --reps 1 --out footprint-600.jsonl > footprint-600.log 2>&1
step n6-soak
cd $M/n6-soak
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
date +%T > started
timeout 8400 python3 run.py --secs 7200 --every 20 --life 40 --out soak.jsonl > soak.log 2>&1
date +%T > finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
step done
