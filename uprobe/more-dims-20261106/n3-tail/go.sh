#!/usr/bin/env bash
# n3: nginx request latency under sslsniff (kernel, fix-final, fix-final-probecheck, none), in a
# container from bpftime-ssl-nginx:20261023 as ssl-nginx-2x2-20261023 (privileged, host pid and
# network, bench/sslsniff/builds mounted). Removed afterwards.
set -u
D=/home/jetson/src/results/uprobe/more-dims-20261106/n3-tail
U=/home/jetson/src/results/uprobe
cd $D
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
date +%T > started
sudo -n docker rm -f n3-tail > /dev/null 2>&1
# The CO-RE sslsniff built for this (BTF) kernel and realtools-20261104's container options
# (default bridge network). A first run (tail-oldsniff.*) used ssl-nginx-2x2's sslsniff from
# before the BTF kernel and --network host.
sudo -n docker run -d --name n3-tail --privileged --pid=host --shm-size=1g -v /sys:/sys \
  -v /lib/modules:/lib/modules:ro -v $D/bench:/bench \
  -v $U/realtools-20261104/ssl-nginx/sslsniff:/sslsniff:ro \
  -v $U/ssl-nginx-2x2-20261023/builds:/builds:ro bpftime-ssl-nginx:20261023 sleep infinity > container-id.txt
timeout 600 sudo -n docker exec n3-tail python3 /bench/run_fair.py --payload 16 --audit --legs kernel,fix-final,fix-final-probecheck --out /bench/audit-16.json > audit-16.log 2>&1
timeout 2400 sudo -n docker exec n3-tail python3 /bench/run_tail.py --out /bench/tail.jsonl > tail.log 2>&1
sudo -n docker rm -f n3-tail > /dev/null 2>&1
date +%T > finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
