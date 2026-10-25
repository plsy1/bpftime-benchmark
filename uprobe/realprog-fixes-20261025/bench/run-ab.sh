#!/bin/bash
# ssl-nginx fair-scope A/B: bca2ebce vs 2448aeaf (run from the host).
set -u
R=/home/jetson/src/results/uprobe/realprog-fixes-20261025
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
sudo -n docker run -d --name ssl-fix --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R/bench:/bench -v $S/builds:/builds:ro -v $S/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity
sudo -n docker exec ssl-fix python3 /bench/run_fair.py --payload 262144 --legs kernel,bca2ebce,2448aeaf --audit --out /bench/audit-256k.json > $R/bench/audit-256k.log 2>&1
for r in r1 r2; do
  sudo -n docker exec ssl-fix python3 /bench/run_fair.py --payload 16 --reps 10 --legs baseline,kernel,bca2ebce,2448aeaf --out /bench/fair-16-$r.json > $R/bench/fair-16-$r.log 2>&1
  sudo -n docker exec ssl-fix python3 /bench/run_fair.py --payload 262144 --reps 10 --legs baseline,kernel,bca2ebce,2448aeaf --out /bench/fair-256k-$r.json > $R/bench/fair-256k-$r.log 2>&1
done
sudo -n docker rm -f ssl-fix
python3 $R/bench/summarize.py > $R/bench/summary.md
date
