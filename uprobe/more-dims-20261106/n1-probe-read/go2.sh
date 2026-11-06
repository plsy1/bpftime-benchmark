#!/usr/bin/env bash
# n1 round 2: the cost again with fix-final-probecheck (checks on) in the same batch; bad
# pointers; the container cells for the checked build.
set -u
cd /home/jetson/src/results/uprobe/more-dims-20261106/n1-probe-read
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock2-before.txt
date +%T > started2
timeout 1800 python3 run.py --matrix cost --backends kernel,fix-final,fix-final-probecheck --reps 3 --secs 3 --out cost2.jsonl > cost2.log 2>&1
timeout 900 python3 run.py --matrix bad --backends kernel,fix-final,fix-final-probecheck --reps 3 --secs 2 --out bad.jsonl > bad.log 2>&1
timeout 600 python3 run.py --matrix container --backends fix-final-probecheck --reps 2 --secs 2 --out container-probecheck.jsonl > container-probecheck.log 2>&1
date +%T > finished2
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock2-after.txt
