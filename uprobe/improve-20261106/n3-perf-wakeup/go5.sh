#!/usr/bin/env bash
# improve-20261106 item 3, round 5: exp-nap2 (the nap is a futex wait registered in `nappers`:
# producers wake a napping consumer only when a buffer fills, perf watermark or drop, a ring half
# full, or on BPF_RB_FORCE_WAKEUP; exp-nap2.diff, not committed). First bursts against the ring
# buffer (output-dims run.py, 4,096 and 8,240-byte reserves, perf 4 KB, bursts of 100 at 4 us, 1
# and 4 threads, 2 reps; kernel, 0f9c3654, exp-nap, exp-nap2: bursts.jsonl), then
# gethostlatency (3 reps) and ssl-nginx (5 reps) as round 4, and the CPU times (cpu-nap2.txt).
set -u
R=/home/jetson/src/results/uprobe/improve-20261106/n3-perf-wakeup
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
R8=/home/jetson/src/results/uprobe/output-dims-20261106/r8-gethostlatency
cd $R
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before5.txt
date +%T > started5
(cd /home/jetson/src/results/uprobe/output-dims-20261106 && timeout 1800 python3 run.py --backends kernel,dev-0f9c3654-probecheck,exp-0f9c3654-nap,exp-0f9c3654-nap2 --objs out-rbres-4096.bpf.o,out-rbres-8240.bpf.o,out-perf-4096.bpf.o --threads 1,4 --reps 2 --secs 4 --workload burst --burst 100 --pause-us 10000 --period-ns 4000 --latency --out $R/bursts.jsonl > $R/bursts.log 2>&1)
(cd $R8 && timeout 900 python3 run.py --backends kernel,dev-0f9c3654-probecheck,exp-0f9c3654-nap2 --reps 3 --out $R/gethost5.jsonl --sendlog $R/gethost5-send.log > $R/gethost5.log 2>&1)
sudo -n docker rm -f imp-ab > /dev/null 2>&1
sudo -n docker run -d --name imp-ab --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > container-id5.txt
timeout 2400 sudo -n docker exec imp-ab python3 /bench/run_fair.py --payload 16 --reps 5 --legs kernel,dev-a48f6777-probecheck,dev-0f9c3654-probecheck,exp-0f9c3654-nap2 --out /bench/ab5-16.json > ab5-16.log 2>&1
sudo -n docker rm -f imp-ab > /dev/null 2>&1
date +%T > finished5
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after5.txt
(cd $R/prof && LEGS="kernel dev-0f9c3654-probecheck exp-0f9c3654-nap2" OUT=cpu-nap2.txt timeout 900 ./cpu.sh > cpu-nap2.log 2>&1)
echo done
