#!/bin/bash
# Audit, 5-tool measurement and full ten rounds for 4d9b8a61 (sequential; machine otherwise idle).
set -u
R=/home/jetson/src/results/uprobe/realprog-rest-20261026
H=/home/jetson/src/realprog-20261025/harness
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-before.txt
cd $R && python3 $H/audit.py --backends kernel,f9d2d9fc,4d9b8a61 --out audit.jsonl > audit.log 2>&1
mkdir -p $R/measure-run && cd $R/measure-run && python3 $H/measure.py --backends none,kernel,f9d2d9fc,4d9b8a61 --out measure.jsonl > measure.log 2>&1
bash $R/full10-4d9b8a61/run-full.sh > $R/full10-4d9b8a61/full-console.log 2>&1
echo "full10 exit $?"
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-after.txt
date
