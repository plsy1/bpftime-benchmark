#!/usr/bin/env bash
# improve-20261106 item 3: perf output wakes the consumer as its attr asks (f5b388cd).
# gethostlatency's delays (output-dims r8 harness) and ssl-nginx throughput (more-dims n3 ab
# harness), against a48f6777 (half-shard rule, same checked build otherwise) and the kernel.
set -u
R=/home/jetson/src/results/uprobe/improve-20261106/n3-perf-wakeup
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
R8=/home/jetson/src/results/uprobe/output-dims-20261106/r8-gethostlatency
cd $R
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
date +%T > started
(cd $R8 && timeout 900 python3 run.py --backends kernel,dev-a48f6777-probecheck,dev-f5b388cd-probecheck --reps 3 --out $R/gethost.jsonl --sendlog $R/gethost-send.log > $R/gethost.log 2>&1)
sudo -n docker rm -f imp-ab > /dev/null 2>&1
sudo -n docker run -d --name imp-ab --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > container-id.txt
timeout 1800 sudo -n docker exec imp-ab python3 /bench/run_fair.py --payload 16 --reps 5 --legs baseline,kernel,dev-a48f6777-probecheck,dev-f5b388cd-probecheck --out /bench/ab-16.json > ab-16.log 2>&1
sudo -n docker rm -f imp-ab > /dev/null 2>&1
date +%T > finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
echo done
