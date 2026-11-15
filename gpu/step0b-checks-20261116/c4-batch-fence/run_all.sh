#!/bin/bash
# C4 real run (main session; quiet board; nothing else on the GPU; clocks
# fixed). Part A: cost (bfc, gated, interleaved rounds). Part B: correctness
# (pubc matrix). Then analyze_cost.py and analyze_pub.py.
#   ./run_all.sh [full|quick|smoke]
# Whole-script bound TOTAL_S (default full 1500, quick 600, smoke 150): every
# call runs under timeout -k 5 with its own budget, budgets are cut to what is
# left, and calls are skipped (logged SKIPPED) once too little remains, so no
# outer timeout is needed (it would orphan a running program).
# Expected wall time (README): full about 17 minutes, quick about 6, smoke
# about 1 (51 s measured).
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
PROFILE=${1:-full}
case "$PROFILE" in
full) DEF_TOTAL=1500 ;;
quick) DEF_TOTAL=600 ;;
smoke) DEF_TOTAL=150 ;;
*) echo "profile: full|quick|smoke"; exit 2 ;;
esac
TOTAL_S=${TOTAL_S:-$DEF_TOTAL}
T0=$(date +%s)
OUT=$D/results/run-$(date +%Y%m%d-%H%M%S)-$PROFILE
mkdir -p "$OUT/cost" "$OUT/pubc"
shm_list() { ls /dev/shm | grep '^step0b_' || echo "(no step0b_ segments)"; }
board() {
	echo "cpu5 cur freq: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
	echo "gpu cur freq: $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
	echo "gpu load (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)"
	echo "busiest processes:"; ps -eo pid,user,pcpu,etime,comm --sort=-pcpu | head -6
}
trap 'echo "end $(date "+%F %T")" >> "$OUT/meta.txt"; board >> "$OUT/meta.txt"; echo "shm after:" >> "$OUT/meta.txt"; shm_list >> "$OUT/meta.txt"' EXIT
{
	echo "start $(date '+%F %T') profile $PROFILE TOTAL_S $TOTAL_S"
	echo "host $(hostname) kernel $(uname -r)"
	board
	echo "shm before:"; shm_list
	echo "sass check: $(tail -1 build/sass_check.txt 2>/dev/null)"
} | tee "$OUT/meta.txt"
if ! grep -q 'ALL OK' build/sass_check.txt 2>/dev/null; then
	echo "build/sass_check.txt is not ALL OK: run ./build.sh first" | tee -a "$OUT/meta.txt"
	exit 3
fi
# the binaries and the SASS check must be newer than every source they come from
for b in build/bfc build/pubc build/kernels.cubin build/sass_check.txt; do
	for s in src/kernels.cu src/bfc.cpp src/pubc.cu common/step0b_regmem.h sass_check.py; do
		if [ ! -e "$b" ] || [ "$s" -nt "$b" ]; then
			echo "$b is missing or older than $s: run ./build.sh first" | tee -a "$OUT/meta.txt"
			exit 3
		fi
	done
done
if [ "$PROFILE" = full ] && [ "$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)" != 1728000 ] \
	&& [ "${ALLOW_UNCLOCKED:-0}" != 1 ]; then
	echo "cpu5 is not at 1728000 kHz (clocks not fixed): refusing a full run (ALLOW_UNCLOCKED=1 overrides)" | tee -a "$OUT/meta.txt"
	exit 4
fi
left() { echo $((TOTAL_S - ($(date +%s) - T0))); }

# ---------------------------------------------------------------- A. cost
case "$PROFILE" in
full) ROUNDS=10; SHAPES=full,anchor,large; GS=1,2,4,8,16,32,48,96,384,1536,8192; CW=480 ;;
quick) ROUNDS=3; SHAPES=full,anchor,large; GS=1,2,4,8,16,32,48,96,384,1536,8192; CW=200 ;;
smoke) ROUNDS=1; SHAPES=anchor; GS=1,48,384,8192; CW=40 ;;
esac
# keep bfc inside the whole-script bound when TOTAL_S is set lower than the profile's default
[ $((CW + 120)) -gt "$(left)" ] && CW=$(( $(left) - 120 ))
[ "$CW" -lt 10 ] && CW=10
echo "== cost $(date '+%T'): rounds $ROUNDS shapes $SHAPES G $GS max-wall ${CW}s" | tee -a "$OUT/meta.txt"
timeout -k 5 $((CW + 120)) ./build/bfc --out "$OUT/cost" --rounds "$ROUNDS" --shapes "$SHAPES" \
	--gs "$GS" --target-ms 4 --max-wall-s "$CW" > "$OUT/cost/bfc.log" 2>&1
echo "bfc exit $? at $(date '+%T')" | tee -a "$OUT/meta.txt"
python3 -I analyze_cost.py "$OUT/cost" > "$OUT/cost/analyze.log" 2>&1 && echo "cost summary: $OUT/cost/summary.md"

# ---------------------------------------------------------------- B. correctness
# budgets (s): MAIN = protocols at every G; CTL = none/sysonly/nosys; EARLY;
# EXTRA = the other protocols at G = 48; TF = fence timing; CONTIG
case "$PROFILE" in
full) MAIN=20; CTL=8; EARLY=4; EXTRA=10; TF=5; CONTIG=5; PGS="1 8 48 384" ;;
quick) MAIN=5; CTL=3; EARLY=2; EXTRA=3; TF=2; CONTIG=2; PGS="1 8 48 384" ;;
smoke) MAIN=1; CTL=1; EARLY=1; EXTRA=1; TF=1; CONTIG=1; PGS="1 48" ;;
esac
# run NAME BUDGET LAYOUT VARIANT G [extra pubc args]
run() {
	local name=$1 budget=$2 layout=$3 variant=$4 g=$5; shift 5
	local l
	l=$(( $(left) - 40 ))
	if [ "$l" -lt 5 ]; then
		echo "== $name SKIPPED: whole-script bound ${TOTAL_S}s reached" | tee -a "$OUT/meta.txt"
		return
	fi
	[ "$budget" -gt "$l" ] && budget=$l
	echo "== $name $(date '+%T') budget ${budget}s" >> "$OUT/meta.txt"
	local rc try
	for try in 1 2; do
		timeout -k 5 $((budget + 30)) ./build/pubc --layout "$layout" --variant "$variant" --G "$g" \
			--records 100000000000 --budget-s "$budget" "$@" --out "$OUT/pubc/$name.json" \
			> /dev/null 2> "$OUT/pubc/$name.stderr"
		rc=$?
		# another process holding the GPU at start-up: retry once
		if [ "$rc" -ne 0 ] && grep -q 'busy or unavailable' "$OUT/pubc/$name.stderr"; then
			echo "   rc $rc (device busy), retry" >> "$OUT/meta.txt"
			sleep 2
			continue
		fi
		break
	done
	echo "   rc $rc" >> "$OUT/meta.txt"
	[ -s "$OUT/pubc/$name.stderr" ] || rm -f "$OUT/pubc/$name.stderr"
}
for L in w32 l8; do
	for G in $PGS; do
		run "${L}_early_G$G" "$EARLY" "$L" early "$G"
		run "${L}_none_G$G" "$CTL" "$L" none "$G"
		run "${L}_ar_sc_G$G" "$MAIN" "$L" ar_sc "$G"
		run "${L}_ar_aa_G$G" "$MAIN" "$L" ar_aa "$G"
		run "${L}_sysonly_G$G" "$CTL" "$L" sysonly "$G"
		run "${L}_nosys_G$G" "$CTL" "$L" nosys "$G"
	done
	X="tf_sc rel_sc ar_ar rel_ar rmw_ar"
	[ "$L" = l8 ] && X="$X ar32_sc ar32_ar ar32_aa"
	for V in $X; do
		run "${L}_${V}_G48" "$EXTRA" "$L" "$V" 48
	done
	run "${L}_ar_sc_G48_timed" "$TF" "$L" ar_sc 48 --time-fence
	run "${L}_ar_aa_G48_timed" "$TF" "$L" ar_aa 48 --time-fence
done
run w32_none_G8_contig "$CONTIG" w32 none 8 --contiguous
run w32_ar_aa_G8_contig "$CONTIG" w32 ar_aa 8 --contiguous
python3 -I analyze_pub.py "$OUT/pubc" > "$OUT/pubc/analyze.log" 2>&1 && echo "correctness summary: $OUT/pubc/summary.md"
echo "total $(( $(date +%s) - T0 )) s" | tee -a "$OUT/meta.txt"
