#!/bin/bash
# e2-cost real run (main session, quiet board, clocks checked beforehand).
#   ./run_all.sh [ROUNDS=10]
# Total timeout 900 s (the program itself stops starting rounds after 780 s).
# Expected wall time: see README ("Expected wall time").
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
ROUNDS=${1:-10}
OUT=$D/results/run-$(date +%Y%m%d-%H%M%S)
mkdir -p "$OUT"
shm_list() { ls /dev/shm | grep '^step0_' || echo "(no step0_ segments)"; }
trap 'echo "shm after:"; shm_list | tee -a "$OUT/meta.txt"' EXIT
{
	echo "start $(date '+%F %T')"
	echo "host $(hostname) kernel $(uname -r)"
	echo "cpu5 cur freq: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
	echo "gpu cur freq: $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
	echo "gpu load (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)"
	echo "shm before:"; shm_list
	echo "sass check: $(tail -1 build/sass_check.txt)"
} | tee "$OUT/meta.txt"
timeout -k 5 900 ./build/bench2 --out "$OUT" --rounds "$ROUNDS" --target-ms 4 --max-wall-s 780 \
	2>&1 | tee "$OUT/bench2.log"
echo "bench2 exit: ${PIPESTATUS[0]}" | tee -a "$OUT/meta.txt"
echo "gpu load after (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)" >> "$OUT/meta.txt"
echo "end $(date '+%F %T')" >> "$OUT/meta.txt"
python3 -I analyze.py "$OUT" > "$OUT/analyze.log" 2>&1 && echo "summary: $OUT/summary.md"
