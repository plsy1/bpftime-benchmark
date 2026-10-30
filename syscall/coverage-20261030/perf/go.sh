#!/usr/bin/env bash
# Old vs new transformer, alternating per round; none once per round.
set -u
D=/home/jetson/src/results/syscall/coverage-20261030/perf
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
for r in 0 1 2 3 4; do
  python3 $D/run_layers.py --legs none --rounds 1 --out $D/none.jsonl > /dev/null 2>&1
  for v in old new; do
    BPFTIME_TRANSFORMER=$D/transformer-$v.so python3 $D/run_layers.py --legs rewrite,agent-other,agent-exit,agent-count --rounds 1 --out $D/$v.jsonl > /dev/null 2>&1
  done
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
