#!/bin/bash
# C3 real run (main session, quiet board, clocks checked beforehand; build
# first with ./build.sh, CPU only).
#   ./run_all.sh [ROUNDS=10] [TAG=run]     -> results/<TAG>-<time>/
# Run it twice, serially, for the two passes, then
#   python3 -I compare_runs.py results/run-<1> results/run-<2>
# Total timeout 300 s (bench3 itself stops starting rounds after 240 s).
# Expected wall time: about 8 s of validation and calibration plus about
# 1.3 s per round (318 timed items), so about 25 s for 10 rounds.
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
ROUNDS=${1:-10}
TAG=${2:-run}
OUT=$D/results/$TAG-$(date +%Y%m%d-%H%M%S)
if [ ! -x build/bench3 ] || ! grep -q "ALL OK" build/sass_check.txt 2>/dev/null; then
	echo "build missing or SASS check not ALL OK: run ./build.sh first" >&2
	exit 2
fi
mkdir -p "$OUT"
shm_list() { ls /dev/shm | grep '^step0b_' || echo "(no step0b_ segments)"; }
trap 'echo "shm after:"; shm_list | tee -a "$OUT/meta.txt"' EXIT
{
	echo "start $(date '+%F %T')"
	echo "host $(hostname) kernel $(uname -r)"
	echo "cpu5 cur freq: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
	echo "gpu cur freq: $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
	echo "gpu load (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)"
	echo "shm before:"; shm_list
	echo "sass check: $(tail -1 build/sass_check.txt)"
	echo "refF identical to step 0's ref1512.ptx: $(python3 -I -c 'import json;print(json.load(open("build/ref/variants.meta.json"))["refF_identical_to_step0_ref1512_ptx"])')"
	sha256sum build/bench3 build/kernels.cubin build/ref/refF.cubin build/ref/refN.cubin build/ref/refX.cubin
} | tee "$OUT/meta.txt"
timeout -k 5 300 ./build/bench3 --out "$OUT" --rounds "$ROUNDS" --target-ms 4 --max-wall-s 240 \
	2>&1 | tee "$OUT/bench3.log"
RC=${PIPESTATUS[0]}
echo "bench3 exit: $RC" | tee -a "$OUT/meta.txt"
{
	echo "gpu load after (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)"
	echo "gpu cur freq after: $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
	echo "cpu5 cur freq after: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
	echo "end $(date '+%F %T')"
} >> "$OUT/meta.txt"
python3 -I analyze.py "$OUT" > "$OUT/analyze.log" 2>&1 && echo "summary: $OUT/summary.md"
ARC=$?
# bench3 exits 6 when a validation failed (the summary then says INVALID RUN),
# 124/137 on the timeout; any non-zero status makes this script fail too
if [ "$RC" -ne 0 ]; then
	echo "bench3 exit status $RC: see $OUT/bench3.log" >&2
	exit "$RC"
fi
exit "$ARC"
