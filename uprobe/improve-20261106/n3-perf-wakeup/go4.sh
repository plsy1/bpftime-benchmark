#!/usr/bin/env bash
# improve-20261106 item 3, round 4: exp-nap (for 2 ms after events the consumer naps 100 us at a
# time without registering as a waiter, so producers make no system call; exp-nap.diff, not
# committed). gethostlatency (3 reps): kernel, 0f9c3654 (head: every sample), exp-0f9c3654-nap;
# ssl-nginx (5 reps): kernel, a48f6777 (half-shard rule), 0f9c3654, exp-0f9c3654-nap; then
# sslsniff's and nginx's CPU time (prof/cpu.sh, cpu-nap.txt).
set -u
R=/home/jetson/src/results/uprobe/improve-20261106/n3-perf-wakeup
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
R8=/home/jetson/src/results/uprobe/output-dims-20261106/r8-gethostlatency
cd $R
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before4.txt
date +%T > started4
(cd $R8 && timeout 900 python3 run.py --backends kernel,dev-0f9c3654-probecheck,exp-0f9c3654-nap --reps 3 --out $R/gethost4.jsonl --sendlog $R/gethost4-send.log > $R/gethost4.log 2>&1)
sudo -n docker rm -f imp-ab > /dev/null 2>&1
sudo -n docker run -d --name imp-ab --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > container-id4.txt
timeout 2400 sudo -n docker exec imp-ab python3 /bench/run_fair.py --payload 16 --reps 5 --legs kernel,dev-a48f6777-probecheck,dev-0f9c3654-probecheck,exp-0f9c3654-nap --out /bench/ab4-16.json > ab4-16.log 2>&1
sudo -n docker rm -f imp-ab > /dev/null 2>&1
date +%T > finished4
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after4.txt
(cd $R/prof && LEGS="kernel dev-0f9c3654-probecheck exp-0f9c3654-nap" OUT=cpu-nap.txt timeout 900 ./cpu.sh > cpu-nap.log 2>&1)
echo done
