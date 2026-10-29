#!/bin/bash
cd /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/sysrun
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
timeout 1800 sudo -n env HOME=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/emptyhome python3 runbench.py
echo "exit $?"
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
