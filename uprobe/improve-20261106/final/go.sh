#!/usr/bin/env bash
# improve-20261106 final batch: the kernel, fix-final-probecheck (d46ceb8f with probe checks on:
# before) and dev-3f8981e4-probecheck (after items 1-7) through the harnesses the problems came
# from. Steps and times in go.log; clock files before and after.
set -u
F=/home/jetson/src/results/uprobe/improve-20261106/final
O=/home/jetson/src/results/uprobe/output-dims-20261106
M=/home/jetson/src/results/uprobe/more-dims-20261106
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
L=$F/go.log
B=kernel,fix-final-probecheck,dev-3f8981e4-probecheck
step() { echo "$(date +%T) $1" >> $L; }
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $F/clock-before.txt
step padding
cd $O
timeout 2400 python3 run.py --backends $B --objs out-rbres-4096.bpf.o,out-rbres-4088.bpf.o,out-rbres-256.bpf.o,out-rbres-248.bpf.o,out-rbres-8240.bpf.o,out-rbres-8184.bpf.o --threads 1,4 --reps 2 --secs 4 --workload burst --burst 100 --pause-us 10000 --period-ns 4000 --latency --out $F/padding.jsonl > $F/padding.log 2>&1
step varsize
timeout 1800 python3 run.py --backends $B --objs out-perf-var.bpf.o,out-rbout-var.bpf.o,out-rbres-var.bpf.o --threads 1,4 --reps 2 --secs 4 --workload burst --burst 1000 --pause-us 10000 --period-ns 4000 --latency --out $F/varsize.jsonl > $F/varsize.log 2>&1
step perf-latency
timeout 1800 python3 run.py --backends $B --objs out-perf-256.bpf.o,out-perf-4096.bpf.o --threads 1,4 --reps 2 --secs 4 --workload burst --burst 100 --pause-us 10000 --period-ns 4000 --latency --out $F/perf.jsonl > $F/perf.log 2>&1
step steady
timeout 1800 python3 run.py --backends $B --objs out-control.bpf.o,out-rbres-8.bpf.o,out-perf-8.bpf.o --threads 1,4 --reps 3 --secs 4 --workload burst --burst 10000 --pause-us 0 --out $F/steady.jsonl > $F/steady.log 2>&1
timeout 1800 python3 run.py --backends $B --objs out-control.bpf.o,out-rbres-8.bpf.o,out-perf-8.bpf.o --threads 1 --procs 4 --reps 3 --secs 4 --workload burst --burst 10000 --pause-us 0 --out $F/steady.jsonl >> $F/steady.log 2>&1
step oversub
timeout 2400 python3 run.py --backends $B --objs out-rbres-256.bpf.o,out-perf-256.bpf.o --threads 1 --procs 64 --reps 2 --secs 6 --workload burst --burst 10 --pause-us 10000 --period-ns 4000 --latency --call-hist --out $F/oversub.jsonl > $F/oversub.log 2>&1
step gethostlatency
cd $O/r8-gethostlatency
timeout 900 python3 run.py --backends $B --reps 3 --out $F/gethost.jsonl --sendlog $F/gethost-send.log > $F/gethost.log 2>&1
step opensnoop
cd $F/opensnoop
timeout 1800 python3 run.py $B 2 results.jsonl > run.log 2>&1
step startup
cd $M/n2-startup
timeout 2400 python3 run.py --builds fix-final-probecheck,dev-3f8981e4-probecheck --out $F/startup.jsonl > $F/startup.log 2>&1
step many-probes
cd $M/n4-many-probes
timeout 3600 python3 run.py --backends none,kernel,fix-final-probecheck,dev-3f8981e4-probecheck --out $F/many.jsonl > $F/many.log 2>&1
step footprint
cd $M/n5-footprint
timeout 2400 python3 run.py --builds fix-final-probecheck,dev-3f8981e4-probecheck --hold 10 --reps 1 --out $F/footprint.jsonl > $F/footprint.log 2>&1
timeout 900 python3 run.py --objs out-perf-256.bpf.o --threads 600 --hold 5 --reps 1 --builds fix-final-probecheck,dev-3f8981e4-probecheck --out $F/footprint-600.jsonl > $F/footprint-600.log 2>&1
step ssl-nginx
sudo -n docker rm -f imp-final > /dev/null 2>&1
sudo -n docker run -d --name imp-final --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $F/ssl:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > $F/ssl/container-id.txt
timeout 2400 sudo -n docker exec imp-final python3 /bench/run_fair.py --payload 16 --reps 5 --legs baseline,kernel,fix-final-probecheck,dev-3f8981e4-probecheck --out /bench/ab-16.json > $F/ssl/ab-16.log 2>&1
sudo -n docker rm -f imp-final > /dev/null 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $F/clock-after.txt
step done
