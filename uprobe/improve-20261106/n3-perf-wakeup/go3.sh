#!/usr/bin/env bash
# improve-20261106 item 3, round 3: exp-poll2 (the consumer polls for 200 us after events,
# sched_yield between polls; exp-poll2.diff, not committed). gethostlatency (3 reps): kernel,
# 0f9c3654 (head: every sample), exp-0f9c3654-poll2; ssl-nginx (5 reps): kernel, a48f6777
# (half-shard rule), 0f9c3654, exp-0f9c3654-poll2.
set -u
R=/home/jetson/src/results/uprobe/improve-20261106/n3-perf-wakeup
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
R8=/home/jetson/src/results/uprobe/output-dims-20261106/r8-gethostlatency
cd $R
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before3.txt
date +%T > started3
(cd $R8 && timeout 900 python3 run.py --backends kernel,dev-0f9c3654-probecheck,exp-0f9c3654-poll2 --reps 3 --out $R/gethost3.jsonl --sendlog $R/gethost3-send.log > $R/gethost3.log 2>&1)
sudo -n docker rm -f imp-ab > /dev/null 2>&1
sudo -n docker run -d --name imp-ab --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > container-id3.txt
timeout 2400 sudo -n docker exec imp-ab python3 /bench/run_fair.py --payload 16 --reps 5 --legs kernel,dev-a48f6777-probecheck,dev-0f9c3654-probecheck,exp-0f9c3654-poll2 --out /bench/ab3-16.json > ab3-16.log 2>&1
sudo -n docker rm -f imp-ab > /dev/null 2>&1
date +%T > finished3
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after3.txt
echo done
