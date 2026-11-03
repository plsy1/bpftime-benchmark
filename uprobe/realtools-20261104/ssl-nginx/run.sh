#!/bin/bash
# ssl-nginx fair-scope round (11/04, real-tool validation of the chained hash map): baseline, kernel,
# dev a4ac0d31 (before the hash redesign), dev 7ad5a152. Every leg uses the same sslsniff, now the unmodified CO-RE build of the bpftime
# example (sslsniff/, sha256.txt); the previous rounds used the no-BTF build. Run from the host.
set -u
R=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
uname -r > $R/kernel.txt
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-before.txt
sudo -n docker run -d --name ssl-rt --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $R/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity
sudo -n docker exec ssl-rt python3 /bench/run_fair.py --payload 16 --legs kernel,dev-a4ac0d31,dev-7ad5a152 --audit --out /bench/audit-16.json > $R/audit-16.log 2>&1
sudo -n docker exec ssl-rt python3 /bench/run_fair.py --payload 262144 --legs kernel,dev-a4ac0d31,dev-7ad5a152 --audit --out /bench/audit-256k.json > $R/audit-256k.log 2>&1
for r in r1 r2; do
  sudo -n docker exec ssl-rt python3 /bench/run_fair.py --payload 16 --reps 10 --legs baseline,kernel,dev-a4ac0d31,dev-7ad5a152 --out /bench/fair-16-$r.json > $R/fair-16-$r.log 2>&1
  sudo -n docker exec ssl-rt python3 /bench/run_fair.py --payload 262144 --reps 10 --legs baseline,kernel,dev-a4ac0d31,dev-7ad5a152 --out /bench/fair-256k-$r.json > $R/fair-256k-$r.log 2>&1
done
sudo -n docker rm -f ssl-rt
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-after.txt
python3 $R/summarize.py > $R/summary.md
date
