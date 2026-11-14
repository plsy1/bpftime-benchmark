#!/bin/bash
# e4-publish real run (main session; quiet board; no other GPU process).
#   ./run_all.sh [full|quick|smoke]
# Every pub4 call has its own budget (--budget-s, at most 600 s) and runs under
# timeout -k 5 (budget + 60 s). Expected wall time: README "Expected wall time".
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
PROFILE=${1:-full}
OUT=$D/results/run-$(date +%Y%m%d-%H%M%S)-$PROFILE
mkdir -p "$OUT"
shm_list() { ls /dev/shm | grep '^step0_' || echo "(no step0_ segments)"; }
trap 'echo "shm after:"; shm_list | tee -a "$OUT/meta.txt"' EXIT
{
	echo "start $(date '+%F %T') profile $PROFILE"
	echo "host $(hostname) kernel $(uname -r)"
	echo "cpu5 cur freq: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
	echo "gpu cur freq: $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
	echo "gpu load (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)"
	echo "shm before:"; shm_list
	echo "sass check: $(tail -1 build/sass_check.txt)"
} | tee "$OUT/meta.txt"

# Whole-script bound (seconds, default 2400): each call's budget is cut to
# what is left, and calls are skipped once fewer than 90 s remain, so no
# outer timeout (which would orphan a running pub4) is needed.
TOTAL_S=${TOTAL_S:-2400}
T0=$(date +%s)
# run NAME BUDGET_S ARGS...
run() {
	local name=$1 budget=$2; shift 2
	local left=$((TOTAL_S - ($(date +%s) - T0) - 70))
	if [ "$left" -lt 20 ]; then
		echo "== $name SKIPPED: whole-script bound ${TOTAL_S}s reached" | tee -a "$OUT/meta.txt"
		return
	fi
	[ "$budget" -gt "$left" ] && budget=$left
	echo "== $name $(date '+%T') budget ${budget}s: $*" | tee -a "$OUT/meta.txt"
	timeout -k 5 $((budget + 60)) ./build/pub4 "$@" --budget-s "$budget" \
		--out "$OUT/$name.json" > "$OUT/$name.stdout" 2> "$OUT/$name.stderr"
	echo "   exit $? $(date '+%T')" | tee -a "$OUT/meta.txt"
}

# Sizes (smoke rates on a quiet board, 2026-11-15: test 1 stress 3.4-3.8M rounds/s,
# normal 12-16M rounds/s; test 2 52-59K rounds/s).
if [ "$PROFILE" = full ]; then
	A=1000000000; C=200000000; LONG=100000000; OTHER_S=200000000; OTHER=100000000
	CTL=10000000; T2=10000000; T2S=3000000; T2C=1000000
elif [ "$PROFILE" = quick ]; then
	A=100000000; C=20000000; LONG=10000000; OTHER_S=20000000; OTHER=10000000
	CTL=2000000; T2=1000000; T2S=300000; T2C=200000
else # smoke: checks the script end to end, a few seconds per run
	A=2000000; C=1000000; LONG=2000000; OTHER_S=1000000; OTHER=1000000
	CTL=1000000; T2=100000; T2S=50000; T2C=20000
fi
STRESS="--stress --poison"   # payload over 16 GPU lines, reverse reads, consumed (dirtied) lines
B4="--chunk-rounds 4"        # a new launch every 4 rounds: in smoke runs almost every
                             # reordering appeared in the first round of a launch
# Cd's delay = the median duration of A's fence in the first round of a
# launch, measured by Am (A with the fence timed) in the same mode. Prints
# the delay in ns, or 1500 and a note on stderr when Am gave none.
cd_delay() {
	python3 -I -c '
import json, sys
try:
    p50 = json.load(open(sys.argv[1]))["fence_ns"]["first_round"]["p50"]
    assert p50 > 0
    print(p50)
except Exception as e:
    print(1500)
    print("cd_delay: Am result unusable (%s): Cd uncalibrated, 1500 ns" % e, file=sys.stderr)
' "$1"
}
# Test 1: the checker control first, then the protocol and its control, then the rest.
run t1_E_stress     120 --test 1 --variant E  $STRESS $B4 --rounds $CTL
run t1_E            120 --test 1 --variant E          $B4 --rounds $CTL
run t1_A_stress     600 --test 1 --variant A  $STRESS $B4 --rounds $A
run t1_C_stress     300 --test 1 --variant C  $STRESS $B4 --rounds $C
# Am: A with its fence timed (globaltimer around lane 0's membar.sys); sets
# Cd's delay. In smokes the fence took a median 28 us (stress) and 7 us
# (normal) in a launch's first round, not the 1.1 us of e2's 64-thread smoke.
run t1_Am_stress    120 --test 1 --variant Am $STRESS $B4 --rounds $CTL
DELAY_STRESS=$(cd_delay "$OUT/t1_Am_stress.json" 2>>"$OUT/meta.txt")
echo "Cd delay, stress: $DELAY_STRESS ns" | tee -a "$OUT/meta.txt"
# Cd: C plus a fenceless wait as long as A's fence: does time alone hide the race?
run t1_Cd_stress    300 --test 1 --variant Cd $STRESS $B4 --rounds $C --delay-ns $DELAY_STRESS
run t1_A            300 --test 1 --variant A          $B4 --rounds $A
run t1_C            300 --test 1 --variant C          $B4 --rounds $C
run t1_Am           120 --test 1 --variant Am         $B4 --rounds $CTL
DELAY_NORMAL=$(cd_delay "$OUT/t1_Am.json" 2>>"$OUT/meta.txt")
echo "Cd delay, normal: $DELAY_NORMAL ns" | tee -a "$OUT/meta.txt"
run t1_Cd           300 --test 1 --variant Cd         $B4 --rounds $C --delay-ns $DELAY_NORMAL
# informational: a fixed 1.5 us fenceless wait (the earlier, uncalibrated Cd)
run t1_Cd15_stress  120 --test 1 --variant Cd $STRESS $B4 --rounds $CTL --delay-ns 1500
run t1_Cd15         120 --test 1 --variant Cd         $B4 --rounds $CTL --delay-ns 1500
# long launches (about 1 s each): the steady state inside a launch; C showed
# no violation there in smokes, so this mode is expected not to discriminate
run t1_A_long       300 --test 1 --variant A --chunk-s 1 --rounds $LONG
run t1_C_long       300 --test 1 --variant C --chunk-s 1 --rounds $LONG
# The publishing lane writes no payload word (lanes 1-31 do): a fence that
# ordered only its own lane's accesses would order nothing of the payload.
# C0 is A0's power reference. Am0mix times the leader's fence in this layout
# (even warps) against fences of warps with no payload at all (odd warps),
# in one run under the same load: if both take as long, A0's zero cannot be
# separated from the fence's duration.
run t1_A0_stress    300 --test 1 --variant A  $STRESS $B4 --lane0-idle --rounds $OTHER_S
run t1_C0_stress    120 --test 1 --variant C  $STRESS $B4 --lane0-idle --rounds $CTL
run t1_Am0mix_stress 120 --test 1 --variant Am $STRESS $B4 --lane0-idle --odd-warps-idle --rounds $CTL
run t1_B_stress     300 --test 1 --variant B  $STRESS $B4 --rounds $OTHER_S
run t1_D1_stress    300 --test 1 --variant D1 $STRESS $B4 --rounds $OTHER_S
run t1_D2_stress    300 --test 1 --variant D2 $STRESS $B4 --rounds $OTHER_S
run t1_B            300 --test 1 --variant B          $B4 --rounds $OTHER
run t1_D1           300 --test 1 --variant D1         $B4 --rounds $OTHER
run t1_D2           300 --test 1 --variant D2         $B4 --rounds $OTHER
# Test 2: the control (the CPU stores FREE before it resets the payload) first.
run t2_control_cas      120 --test 2 --variant cas      --wrong-order --rounds $T2C
run t2_control_casfence 120 --test 2 --variant casfence --wrong-order --rounds $T2C
run t2_cas          300 --test 2 --variant cas      --rounds $T2
run t2_casfence     300 --test 2 --variant casfence --rounds $T2
run t2_casacq       300 --test 2 --variant casacq   --rounds $T2S
run t2_cas_stress   300 --test 2 --variant cas --stress --rounds $T2S
echo "end $(date '+%F %T')" | tee -a "$OUT/meta.txt"
echo "gpu load after (permille): $(cat /sys/devices/platform/bus@0/17000000.gpu/load 2>/dev/null)" >> "$OUT/meta.txt"
python3 -I analyze.py "$OUT" > "$OUT/analyze.log" 2>&1 && echo "summary: $OUT/summary.md"
