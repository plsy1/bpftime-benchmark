#!/usr/bin/env bash
# Hook cost: a4ac0d31's transformer (frame on the caller's stack) vs the hook
# stack, per write, alternating per round; agent from the current build in both.
set -u
D=/home/jetson/src/results/syscall/realapps-20261102/hook-perf
P=/home/jetson/src/results/syscall/coverage-20261030/perf
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
for r in 0 1 2 3 4 5 6 7 8 9; do
  BPFTIME_TRANSFORMER=$D/transformer-a4ac0d31.so python3 $P/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/before.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$D/transformer-hookstack.so python3 $P/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/after.jsonl > /dev/null 2>&1
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
