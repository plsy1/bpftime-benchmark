#!/bin/bash
# ssl-nginx fair-scope round on the BTF kernel (11/02 evening): baseline, kernel, official 177a1dc,
# dev 68f19c1f and dev a4ac0d31 (per-CPU hash without the map lock), same CO-RE sslsniff for every leg
# (sslsniff/, sha256.txt). Copied from dev-68f19c1f-20261102/ssl-nginx/run.sh. Run from the host.
set -u
R=/home/jetson/src/results/uprobe/dev-a4ac0d31-20261102/ssl-nginx
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
LEGS=kernel,official,dev-68f19c1f,dev-a4ac0d31
uname -r > $R/kernel.txt
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-before.txt
sudo -n docker run -d --name ssl-a4ac --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $R/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity
sudo -n docker exec ssl-a4ac python3 /bench/run_fair.py --payload 16 --legs $LEGS --audit --out /bench/audit-16.json > $R/audit-16.log 2>&1
sudo -n docker exec ssl-a4ac python3 /bench/run_fair.py --payload 262144 --legs $LEGS --audit --out /bench/audit-256k.json > $R/audit-256k.log 2>&1
for r in r1 r2; do
  sudo -n docker exec ssl-a4ac python3 /bench/run_fair.py --payload 16 --reps 10 --legs baseline,$LEGS --out /bench/fair-16-$r.json > $R/fair-16-$r.log 2>&1
  sudo -n docker exec ssl-a4ac python3 /bench/run_fair.py --payload 262144 --reps 10 --legs baseline,$LEGS --out /bench/fair-256k-$r.json > $R/fair-256k-$r.log 2>&1
done
sudo -n docker rm -f ssl-a4ac
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-after.txt
python3 $R/summarize.py > $R/summary.md
date
