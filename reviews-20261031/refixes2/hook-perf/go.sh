#!/usr/bin/env bash
# Hook cost: 9aeb2863's transformer (compare chain for the special calls) vs
# f73a45f1's (bitmap; munmap, mremap, shmat, remap_file_pages out of line),
# per write, alternating per round; agent from the current build in both.
set -u
D=/home/jetson/src/results/reviews-20261031/refixes2/hook-perf
P=/home/jetson/src/results/syscall/coverage-20261030/perf
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
for r in 0 1 2 3 4 5 6 7 8 9; do
  BPFTIME_TRANSFORMER=$D/transformer-9aeb2863.so python3 $P/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/before.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$D/transformer-f73a45f1.so python3 $P/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/after.jsonl > /dev/null 2>&1
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
