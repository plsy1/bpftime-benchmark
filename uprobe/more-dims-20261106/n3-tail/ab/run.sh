#!/usr/bin/env bash
# ssl-nginx throughput A/B across the builds since 11-04's +18.5%, with realtools-20261104's
# exact harness (run_fair.py, container options, wrk -c 100 -d 10 without a script): kernel,
# dev-7ad5a152 (11-04), dev-25f4134f (Gum slots, parking, grace period), dev-b60b99c3 (before
# the fifth review's fixes), fix-final; 16-byte page, 5 reps, order rotated.
set -u
R=/home/jetson/src/results/uprobe/more-dims-20261106/n3-tail/ab
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
cd $R
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
date +%T > started
sudo -n docker rm -f n3-ab > /dev/null 2>&1
sudo -n docker run -d --name n3-ab --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > container-id.txt
timeout 1500 sudo -n docker exec n3-ab python3 /bench/run_fair.py --payload 16 --reps 5 --legs baseline,kernel,dev-7ad5a152,dev-25f4134f,dev-b60b99c3,fix-final --out /bench/ab-16.json > ab-16.log 2>&1
sudo -n docker rm -f n3-ab > /dev/null 2>&1
date +%T > finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
