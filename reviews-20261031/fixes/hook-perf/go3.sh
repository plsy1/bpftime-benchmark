#!/usr/bin/env bash
# Frame alignment check: before (d807ef93), fixes (784-byte frame), fixes832
# (the same with an 832-byte frame: the agent's frames at the same 64-byte
# phase as before). Alternating per round.
set -u
D=/home/jetson/src/results/reviews-20261031/fixes/hook-perf
P=/home/jetson/src/results/syscall/coverage-20261030/perf
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before3.txt
for r in 0 1 2 3 4 5; do
  BPFTIME_TRANSFORMER=$P/transformer-lazy.so python3 $P/run_layers.py --legs agent-other,agent-exit --rounds 1 --out $D/before3.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$D/transformer-fixes.so python3 $P/run_layers.py --legs agent-other,agent-exit --rounds 1 --out $D/fixes3.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$D/transformer-fixes832.so python3 $P/run_layers.py --legs agent-other,agent-exit --rounds 1 --out $D/fixes832.jsonl > /dev/null 2>&1
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after3.txt
