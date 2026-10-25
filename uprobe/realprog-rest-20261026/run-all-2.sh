#!/bin/bash
set -u
R=/home/jetson/src/results/uprobe/realprog-rest-20261026
H=/home/jetson/src/realprog-20261025/harness
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-before-2.txt
cd $R && python3 $H/audit.py --backends kernel,a7d54d7e --out audit-a7d54d7e.jsonl > audit-a7d54d7e.log 2>&1
mkdir -p $R/measure-run-2 && cd $R/measure-run-2 && python3 $H/measure.py --backends kernel,f9d2d9fc,a7d54d7e --out measure.jsonl > measure.log 2>&1
bash $R/full10-a7d54d7e/run-full.sh > $R/full10-a7d54d7e/full-console.log 2>&1
echo "full10 exit $?"
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-after-2.txt
date
