#!/bin/bash
# 11/05: problem 9 after the fix (frozen build $1): the get_stack_id example (bpf_ringbuf_reserve +
# submit) 3 rounds on bpf, official, dev-25f4134f and the fix; opensnoop and gethostlatency once.
set -u
B=$1
N=/home/jetson/src/results/uprobe/ringbuf-fd-20261105
date; cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
cd /home/jetson/src/realprog-20261026-btf/harness
for r in 0 1 2; do
  timeout 600 python3 audit.py --backends kernel,official,dev-25f4134f,$B --tools get_stack_id-example --secs 4 --out $N/stackid/r$r.jsonl >> $N/audit.log 2>&1
  echo "stackid r$r exit $?"
done
timeout 600 python3 audit.py --backends $B --tools opensnoop,gethostlatency --secs 4 --out $N/other/audit.jsonl >> $N/audit.log 2>&1
echo "other exit $?"
date; sudo -n chown -R jetson:jetson $N
