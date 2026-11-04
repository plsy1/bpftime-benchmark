#!/bin/bash
# 11/04: problem 5 of realtools-20261104 (harness gaps). audit.py (copied here as run) now runs
# opensnoop and syscount under the text segment transformer (bpftime start -s), javagc through
# bpftime attach, and counts get_stack_id's victim loops (stdbuf). Backends: bpf, dev-25f4134f
# (frozen with its transformer and CLI), get_stack_id also on official; get_stack_id 3 rounds.
set -u
N=/home/jetson/src/results/uprobe/harness-gaps-20261104
H=/home/jetson/src/realprog-20261026-btf/harness
date; uname -r; cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
cd $H
timeout 900 python3 audit.py --backends kernel,dev-25f4134f --tools opensnoop,syscount,javagc --secs 4 --out $N/audit/audit.jsonl > $N/audit.log 2>&1
echo "audit exit $?"
for r in 0 1 2; do
  timeout 600 python3 audit.py --backends kernel,official,dev-25f4134f --tools get_stack_id-example --secs 4 --out $N/stackid/r$r.jsonl >> $N/audit.log 2>&1
  echo "stackid r$r exit $?"
done
date; sudo -n chown -R jetson:jetson $N
