#!/usr/bin/env bash
# old vs new3 (rescan out of line, direct call), alternating per round.
set -u
D=/home/jetson/src/results/syscall/coverage-20261030/perf
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before3.txt
for r in 0 1 2 3 4; do
  python3 $D/run_layers.py --legs none --rounds 1 --out $D/none3.jsonl > /dev/null 2>&1
  for v in old new3; do
    BPFTIME_TRANSFORMER=$D/transformer-$v.so python3 $D/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/$v-3.jsonl > /dev/null 2>&1
  done
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after3.txt
echo done > $D/finished3
