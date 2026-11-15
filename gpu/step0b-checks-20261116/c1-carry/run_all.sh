#!/bin/bash
# run_all.sh [OUT]: check C1, every part, for the main session on a quiet
# board (clocks checked beforehand; nothing else on the GPU).
#   OUT defaults to runs/<date-time>. Total timeout 900 s: the script re-runs
#   itself under `timeout -k 10 900`, and every step's own timeout is clipped
#   to the time left before that deadline (a nested `timeout` runs in its own
#   process group, so the outer one alone would not stop a running step);
#   every device-side spin has an iteration cap.
# Expected wall time: about 2.5-3 minutes (build 1 min, b 75 s, c 15 s, d 2 s).
# QUICK=1 ./run_all.sh OUT: development only (every GPU step a few seconds;
# the verdicts it prints are not results).
set -u
if [ "${QUICK:-0}" = 1 ]; then
	B_BASIC="launches=1 shapes=full512,w48"; B_GRAPH="replays=20"; B_STREAMS="spin_ms=20 reps=1"
	B_SPIN="spin_ms=100 reps=1"; HOG_S=3; C_ROUNDS=1
else
	B_BASIC="launches=3"; B_GRAPH="replays=1000"; B_STREAMS="spin_ms=30 reps=3"
	B_SPIN="spin_ms=300 reps=2"; HOG_S=10; C_ROUNDS=10
fi
D=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$D/runs/$(date +%Y%m%d-%H%M%S)}
TOTAL_S=900
if [ -z "${C1_UNDER_TOTAL_TIMEOUT:-}" ]; then
	export C1_UNDER_TOTAL_TIMEOUT=1
	C1_DEADLINE=$(( $(date +%s) + TOTAL_S - 5 ))
	export C1_DEADLINE
	exec timeout -k 10 "$TOTAL_S" "$0" "$OUT"
fi
: "${C1_DEADLINE:=$(( $(date +%s) + TOTAL_S - 5 ))}"
[ -e "$OUT" ] && { echo "exists: $OUT"; exit 3; }
mkdir -p "$OUT"
cd "$D" || exit 2
[ "${QUICK:-0}" = 1 ] && echo "QUICK=1: development smoke, not a result" > "$OUT/quick"
# this check's POSIX shm names only (c1bench: step0b_c1_<pid>); other step0b
# checks' segments are left alone
shm_ours() { ls /dev/shm | grep '^step0b_c1_' || true; }
ls /dev/shm > "$OUT/shm_before.txt"
{
	date '+%F %T'; uname -r
	echo "cpu5 cur_freq: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
	echo "gpu cur_freq: $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
	echo "gpu load (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)"
	echo "loadavg: $(cat /proc/loadavg)"
	echo "step0b_ shm before: $(shm_ours | tr '\n' ' ')"
	ps -eo pid,pcpu,etime,args --sort=-pcpu | head -8 | cut -c1-160
} > "$OUT/env.txt"
cat "$OUT/env.txt"
rc=0
# step LOG TIMEOUT CMD...: one bounded step; output to OUT/LOG and the
# terminal; its rc goes to OUT/steps_rc.txt (verdict.py: every GPU step must
# exit 0). The timeout is clipped to the total deadline.
step() {
	local log=$1 t=$2; shift 2
	local left=$(( C1_DEADLINE - $(date +%s) - 10 ))
	echo "== $*" | tee "$OUT/$log"
	if [ "$left" -lt 5 ]; then
		echo "rc=124: no time left before the total deadline: $*" | tee -a "$OUT/$log"
		echo "$log 124" >> "$OUT/steps_rc.txt"; rc=1; return 124
	fi
	[ "$t" -gt "$left" ] && t=$left
	timeout -k 5 "$t" "$@" >> "$OUT/$log" 2>&1
	local r=$?
	echo "$log $r" >> "$OUT/steps_rc.txt"
	tail -3 "$OUT/$log"
	[ $r != 0 ] && { echo "rc=$r: $*" | tee -a "$OUT/$log"; rc=1; }
	return $r
}
# (a) build: dev passes, routing, ptxas, SASS checks (CPU only)
if step build.log 300 ./build.sh; then touch "$OUT/build_ok"; else echo "build failed"; exit 2; fi
cp build/b/sass_check.txt "$OUT/b_sass_check.txt"; cp build/c/sass_check.txt "$OUT/c_sass_check.txt"; cp build/forms.txt "$OUT/a_forms.txt"
cp build/b/ptxas.txt "$OUT/b_ptxas.txt"; cp build/c/ptxas.txt "$OUT/c_ptxas.txt"
# (b) correctness
# shellcheck disable=SC2086 # the option strings are word lists
step b_basic.log 180 ./build/c1check basic "$OUT/basic" $B_BASIC
step b_graph.log 180 ./build/c1check graph "$OUT/graph" $B_GRAPH
step b_streams.log 60 ./build/c1check streams "$OUT/streams" $B_STREAMS
step b_spin_ctl.log 80 ./run_spin.sh "$OUT/spin_ctl" ctl 0 0 $B_SPIN
step b_spin_hog.log 110 ./run_spin.sh "$OUT/spin_hog" hog "$HOG_S" 50 $B_SPIN
QA=""; [ "${QUICK:-0}" = 1 ] && QA=--quick
python3 -I tools/analyze_b.py "$OUT" $QA | tee "$OUT/b_analyze.log"
# (c) cost (timed; 10 rounds as e2)
mkdir -p "$OUT/c"
step c_bench.log 300 ./build/c1bench --out "$OUT/c" --rounds "$C_ROUNDS" --max-wall-s 240
python3 -I tools/analyze_c.py "$OUT/c" > "$OUT/c/analyze.log" 2>&1; tail -25 "$OUT/c/summary.md"
# (d) llama.cpp rms_norm module: occupancy of the compiled variants
mkdir -p "$OUT/d"
K=_Z12rms_norm_f32ILi512ELi1ELb1ELb1ELb0ELb0EEvPKfPfiiilllfS1_lll5uint3S3_S3_S3_S1_lllS3_S3_S3_S3_f
step d_occ.log 60 ./build/occ "$K" 512,1024 build/d/U.cubin build/d/P0.cubin build/d/P0e.cubin \
	build/d/P0c.cubin build/d/P0cd.cubin build/d/E.cubin build/d/N.cubin build/d/H16.cubin \
	build/d/D16.cubin build/d/D64.cubin build/d/D256.cubin
grep -v '^==' "$OUT/d_occ.log" | grep -v '^rc=' > "$OUT/d/occ.csv"
cp build/d/d_compile.json build/d/d_compile.txt "$OUT/d/"
python3 -I tools/analyze_d.py build/d "$OUT/d/occ.csv" "$OUT/d" | tail -12
# shm and CUDA leftovers of processes this script started
echo "step0b_ shm after: $(shm_ours | tr '\n' ' ')" | tee -a "$OUT/env.txt"
for f in $(shm_ours); do echo "removing leftover /dev/shm/$f" | tee -a "$OUT/shm_cleanup.txt"; rm -f "/dev/shm/$f"; done
pids=$(grep -ho '"pid": [0-9]*' "$OUT"/*/meta.json "$OUT"/c/items.json 2>/dev/null | awk '{print $2}'; grep -ho 'hog: pid [0-9]*' "$OUT"/*/hog.log 2>/dev/null | awk '{print $3}')
for p in $pids; do
	for f in /dev/shm/cuda.shm.$(printf '%x' "$(id -u)").$(printf '%x' "$p").*; do
		[ -e "$f" ] && { echo "removing leftover $f" | tee -a "$OUT/shm_cleanup.txt"; rm -f "$f"; }
	done
done
diff "$OUT/shm_before.txt" <(ls /dev/shm) > "$OUT/shm_diff.txt" && echo "shm: unchanged" || { echo "shm changed:"; cat "$OUT/shm_diff.txt"; }
{
	echo "end $(date '+%F %T')"
	echo "cpu5 cur_freq: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
	echo "gpu cur_freq: $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
} >> "$OUT/env.txt"
python3 -I tools/verdict.py "$OUT"
exit $rc
