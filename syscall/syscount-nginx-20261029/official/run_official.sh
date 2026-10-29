#!/bin/bash
# Official benchmark/syscount-nginx/benchmark.py (copied unchanged as runbench.py at the view
# root). No command line on the host may contain the word the script's cleanup pkill -f's.
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
date
timeout 3600 sudo -n docker exec sc-nginx python3 runbench.py
echo "exit $?"
date
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
