#!/bin/bash
# 11/05: problems 10 and 11 after the fixes (frozen build $1 = dev-74d0ab8b). opensnoop and syscount
# under the transformer (bpf and $1), get_stack_id 3 rounds ($1 and dev-6a15a271).
set -u
B=$1
N=/home/jetson/src/results/uprobe/exit-log-poison-20261105
date; cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
cd /home/jetson/src/realprog-20261026-btf/harness
timeout 900 python3 audit.py --backends kernel,$B --tools opensnoop,syscount --secs 4 --out $N/audit/audit.jsonl > $N/audit.log 2>&1
echo "audit exit $?"
for r in 0 1 2; do
  timeout 600 python3 audit.py --backends dev-6a15a271,$B --tools get_stack_id-example --secs 4 --out $N/stackid/r$r.jsonl >> $N/audit.log 2>&1
  echo "stackid r$r exit $?"
done
date; sudo -n chown -R jetson:jetson $N
