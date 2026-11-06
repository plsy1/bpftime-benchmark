#!/usr/bin/env bash
# n1: bpf_probe_read cost by size, and errors with the workload in a container.
set -u
cd /home/jetson/src/results/uprobe/more-dims-20261106/n1-probe-read
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
date +%T > started
timeout 1200 python3 run.py --matrix cost --reps 3 --secs 3 --out cost.jsonl > cost.log 2>&1
timeout 600 python3 run.py --matrix container --reps 3 --secs 2 --out container.jsonl > container.log 2>&1
date +%T > finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
