#!/usr/bin/env bash
# Hook cost: dev-b60b99c3's transformer (the code of 94f9ba8f: busy in TLS) vs
# fix-final's (top-frame marker, stale check, stack reuse), per write,
# alternating per round; agent and server from the candidate build (d46ceb8f) in both.
set -u
D=/home/jetson/src/results/reviews-20261105/fixes/hook-perf
P=/home/jetson/src/results/syscall/coverage-20261030/perf
B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
for r in 0 1 2 3 4 5 6 7 8 9; do
  BPFTIME_TRANSFORMER=$B/dev-b60b99c3/libbpftime-agent-transformer.so timeout 600 python3 $P/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/before.jsonl > /dev/null 2>&1
  BPFTIME_TRANSFORMER=$B/fix-final/libbpftime-agent-transformer.so timeout 600 python3 $P/run_layers.py --legs rewrite,agent-other,agent-exit --rounds 1 --out $D/after.jsonl > /dev/null 2>&1
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
echo done > $D/finished
