#!/usr/bin/env bash
# improve-20261106 item 3, round 2: does the consumer polling for 200 us after events (exp-poll,
# not committed: exp-poll.diff) take the futex wakes off the producer? ssl-nginx (5 reps) and
# gethostlatency (3 reps): kernel, a48f6777 (half-shard rule), 0f9c3654 (head: every sample),
# exp-0f9c3654-poll.
set -u
R=/home/jetson/src/results/uprobe/improve-20261106/n3-perf-wakeup
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
R8=/home/jetson/src/results/uprobe/output-dims-20261106/r8-gethostlatency
cd $R
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before2.txt
date +%T > started2
(cd $R8 && timeout 900 python3 run.py --backends kernel,dev-0f9c3654-probecheck,exp-0f9c3654-poll --reps 3 --out $R/gethost2.jsonl --sendlog $R/gethost2-send.log > $R/gethost2.log 2>&1)
sudo -n docker rm -f imp-ab > /dev/null 2>&1
sudo -n docker run -d --name imp-ab --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > container-id2.txt
timeout 2400 sudo -n docker exec imp-ab python3 /bench/run_fair.py --payload 16 --reps 5 --legs kernel,dev-a48f6777-probecheck,dev-0f9c3654-probecheck,exp-0f9c3654-poll --out /bench/ab2-16.json > ab2-16.log 2>&1
sudo -n docker rm -f imp-ab > /dev/null 2>&1
date +%T > finished2
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after2.txt
echo done
