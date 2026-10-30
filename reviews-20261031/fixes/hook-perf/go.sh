#!/usr/bin/env bash
# Hook cost after the review fixes (784-byte frame, FPCR saved and compared,
# CFI, execve/execveat compares) vs d807ef93 (transformer-lazy.so), per write,
# alternating per round; agent from the current build in both.
set -u
D=/home/jetson/src/results/reviews-20261031/fixes/hook-perf
P=/home/jetson/src/results/syscall/coverage-20261030/perf
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
for r in 0 1 2 3 4; do
  python3 $P/run_layers.py --legs none --rounds 1 --out $D/none.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$P/transformer-lazy.so python3 $P/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/before.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$D/transformer-fixes.so python3 $P/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/fixes.jsonl > /dev/null 2>&1
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
