#!/bin/bash
# 11/05: dead code by frozen read-only constants (frozen build $1 = dev-b60b99c3). The whole audit
# (every harness tool, single thread) on bpf, dev-74d0ab8b and $1: syscount and opensnoop should
# now record, nothing else should change.
set -u
B=$1
N=/home/jetson/src/results/uprobe/rodata-dead-code-20261105
date; cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
cd /home/jetson/src/realprog-20261026-btf/harness
timeout 3000 python3 audit.py --backends kernel,dev-74d0ab8b,$B --secs 4 --out $N/audit/audit.jsonl > $N/audit.log 2>&1
echo "audit exit $?"
date; sudo -n chown -R jetson:jetson $N
