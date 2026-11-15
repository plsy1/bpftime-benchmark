#!/bin/bash
# run_all.sh [OUT]: check C2 (step 0b), everything, for the main session on a quiet board.
#   SMOKE=1 ./run_all.sh OUT   a scaled-down development run (each GPU step well under 30 s)
# Total timeout: the script re-executes itself under `timeout` (TOTAL_S, default 2400 s); every
# GPU step has its own `timeout -k 5`. Nothing here uses LD_PRELOAD or POSIX shm.
# Expected wall time (quiet board): build about 1 min, cost bench about 1-2 min, correctness
# about 3-5 min, holds about 45 s (the hog runs 30 s); about 7-10 min in total (README "Commands").
set -u
D=$(cd "$(dirname "$0")" && pwd)
TOTAL_S=${TOTAL_S:-2400}
if [ -z "${C2_RUN_ALL_INNER:-}" ]; then
	export C2_RUN_ALL_INNER=1
	exec timeout -k 10 "$TOTAL_S" "$0" "$@"
fi
OUT=${1:-$D/runs/$(date +%Y%m%d-%H%M%S)}
case "$OUT" in /*) ;; *) OUT="$PWD/$OUT" ;; esac
SMOKE=${SMOKE:-0}
[ -e "$OUT" ] && { echo "exists: $OUT"; exit 3; }
mkdir -p "$OUT"
cd "$D" || exit 2
exec > >(trap "" TERM INT; exec tee -a "$OUT/run_all.log") 2>&1 # tee outlives the trap message

board() {
	echo "== board $1 $(date '+%F %T')"
	echo "cpu5 cur freq: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
	echo "gpu cur freq: $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
	echo "gpu load (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)"
	echo "loadavg: $(cat /proc/loadavg)"
	ps -eo pid,pcpu,etime,args --sort=-pcpu | head -8 | cut -c1-160
}
board before | tee "$OUT/board_before.txt"
ls /dev/shm > "$OUT/shm_before.txt"
uname -a > "$OUT/env.txt"

if [ "$SMOKE" = 1 ]; then
	CHK_SCALE="--min-claims 5000 --ctl-min-claims 2000 --aba-min-claims 5000 --max-launches 20 --reuse-min 1 --full-reps 1 --stream-launches 6 --graph-replays 20 --mask-launches 3"
	HOLD_ARGS="--hold-ms 2 --hold-launches 1"
	HOG_S=8
	BENCH_ARGS="--rounds 1 --target-ms 1 --shapes full,anchor"
	T_NATIVE=30; T_JIT=30; T_HOLD=30; T_BENCH=30
else
	CHK_SCALE="--min-claims 100000 --ctl-min-claims 20000 --aba-min-claims 1000000 --max-launches 600 --full-reps 3 --stream-launches 30 --graph-replays 200 --mask-launches 20"
	HOLD_ARGS="--hold-ms 8 --hold-launches 3"
	HOG_S=30
	BENCH_ARGS="--rounds 10 --target-ms 4 --max-wall-s 780"
	T_NATIVE=1500; T_JIT=400; T_HOLD=150; T_BENCH=900
fi
rc=0
# The total timeout must also stop the step that is running: each `timeout` below puts itself in
# its own process group, so the outer timeout's signal reaches only this script. Steps therefore
# run in the background and are waited for; on TERM/INT the trap signals the running step's
# `timeout` (which passes the signal on to its command and SIGKILLs it 5 s later) and the hog.
CUR=""
HP=""
# CUDA can leave /dev/shm/cuda.shm.<uid hex>.<pid hex>.* behind when a process is killed:
# remove only entries whose pid is one of ours (c2chk, c2bench and the hog record their pids at start)
cleanup_cuda_shm() {
	local pids p f
	pids=$(grep -ho '"pid": [0-9]*' "$OUT"/*/meta_*.json 2>/dev/null | awk '{print $2}'; grep -ho 'hog: pid [0-9]*' "$OUT"/hold_hog/hog.log 2>/dev/null | awk '{print $3}')
	for p in $pids; do
		for f in /dev/shm/cuda.shm.$(printf '%x' "$(id -u)").$(printf '%x' "$p").*; do
			[ -e "$f" ] && { echo "removing leftover $f" | tee -a "$OUT/shm_cleanup.txt"; rm -f "$f"; }
		done
	done
	diff "$OUT/shm_before.txt" <(ls /dev/shm) > "$OUT/shm_diff.txt" && echo "shm: unchanged" || { echo "shm changed:"; cat "$OUT/shm_diff.txt"; }
}
on_term() {
	echo "run_all: stopped by a signal (total timeout $TOTAL_S s?), stopping the running step"
	[ -n "$CUR" ] && kill -TERM "$CUR" 2>/dev/null
	[ -n "$HP" ] && kill -TERM "$HP" 2>/dev/null
	[ -n "$CUR" ] && wait "$CUR" 2>/dev/null
	[ -n "$HP" ] && wait "$HP" 2>/dev/null
	cleanup_cuda_shm
	exit 124
}
trap on_term TERM INT
# step NAME TIMEOUT CMD...: one bounded step; its output goes to OUT/NAME.log and the terminal
step() {
	local name=$1 t=$2
	shift 2
	echo "== step $name ($(date '+%T'), timeout $t s): $*"
	timeout -k 5 "$t" "$@" > "$OUT/$name.log" 2>&1 &
	CUR=$!
	wait "$CUR"
	local r=$?
	CUR=""
	tail -n 4 "$OUT/$name.log"
	echo "== step $name rc=$r ($(date '+%T'))"
	[ $r = 0 ] || rc=1
	return $r
}

step build 600 ./build.sh || { echo "build failed"; exit 2; }
cp -r build/sass_check.txt build/sass_check.json build/ref/refprobe.meta.json "$OUT/"
grep -q "^RESULT PASS" build/sass_check.txt || { echo "SASS/PTX check failed, see $OUT/sass_check.txt"; exit 2; }
step selftest 60 python3 -I analyze_chk.py --selftest || exit 2

# 1. cost first, on the quietest board (step0/e2-cost's bench2 method; step 0's pass 2 order)
mkdir -p "$OUT/bench"
step bench "$T_BENCH" ./build/c2bench --out "$OUT/bench" $BENCH_ARGS

# 2. correctness, native (nvcc cubin): reproducer, divergent cases, ABA, full occupancy, streams, graph
step native "$T_NATIVE" ./build/c2chk --module build/kernels.cubin --path native --out "$OUT/native" $CHK_SCALE
# 3. correctness, driver JIT of plain-call PTX (as bpftime loads patched modules): a subset
step jit "$T_JIT" ./build/c2chk --module build/kernels_jit.ptx --path jit --out "$OUT/jit" \
	--cases mask,div --pools bmsm,stkglob,lnsm,lfsm --exits o_lane,oc_lane,o_first,oc_first $CHK_SCALE
# 4. holds of milliseconds without a second GPU process (reported only)
step hold_ctl "$T_HOLD" ./build/c2chk --module build/kernels.cubin --path native --out "$OUT/hold_ctl" \
	--cases hold --exits o_lane,r_lane,oc_lane $HOLD_ARGS
# 5. the same holds while step0/e1-ids's hog keeps a second context busy (time-slicing)
mkdir -p "$OUT/hold_hog"
rm -f "$OUT/hold_hog/hog.ready"
timeout -k 5 $((HOG_S + 25)) ./build/hog "$HOG_S" 50 "$OUT/hold_hog/hog.ready" "$OUT/hold_hog" > "$OUT/hold_hog/hog.log" 2>&1 &
HP=$!
echo "$HP" > "$OUT/hold_hog/hog.pid"
ok=0
for i in $(seq 1 100); do
	[ -e "$OUT/hold_hog/hog.ready" ] && { ok=1; break; }
	kill -0 "$HP" 2>/dev/null || break
	sleep 0.1
done
if [ $ok = 1 ]; then
	sleep 0.5
	step hold_hog "$T_HOLD" ./build/c2chk --module build/kernels.cubin --path native --out "$OUT/hold_hog" \
		--cases hold --exits o_lane,r_lane,oc_lane $HOLD_ARGS
	if kill -0 "$HP" 2>/dev/null; then echo "hog_alive_at_end=1"; else echo "hog_alive_at_end=0"; fi > "$OUT/hold_hog/hog_alive.txt"
else
	echo "hog not ready" | tee "$OUT/hold_hog/hog_alive.txt"
	rc=1
fi
wait "$HP"
echo "hog rc=$?" >> "$OUT/hold_hog/hog.log"
HP=""
cat "$OUT/hold_hog/hog_alive.txt"
# 6. analysis
timeout -k 5 120 python3 -I analyze_chk.py "$OUT" > "$OUT/analyze_chk.log" 2>&1 || rc=1
timeout -k 5 120 python3 -I analyze_cost.py "$OUT/bench" "$OUT/chk_summary.json" > "$OUT/analyze_cost.log" 2>&1 || rc=1
head -12 "$OUT/chk_summary.md"
sed -n '1,12p' "$OUT/bench/cost_summary.md" 2>/dev/null

board after | tee "$OUT/board_after.txt"
cleanup_cuda_shm
echo "run_all rc=$rc, out $OUT"
exit $rc
