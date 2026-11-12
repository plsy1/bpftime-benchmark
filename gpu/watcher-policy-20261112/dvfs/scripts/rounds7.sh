#!/usr/bin/env bash
# Clock-regime rounds: rounds7.sh FIRST LAST. Each round runs every cell under both clock regimes
# (clocks.sh: on = jetson_clocks as in every earlier round, off = DVFS and idle states), the
# block order alternating (odd rounds off then on, even rounds on then off). Sets P, W (dev
# 46f5a7cb, WFE), D (dev 52126f33, AC3), AC3 / AC2 / AC4 (the experimental build's window rules)
# and M100, shuffled per round, regime and cell (seeded). Cells: name probe load spin_us launches
# threads jitter_us gap_us graph procs. Whatever happens, the board goes back to jetson_clocks.
set -u
G=/home/jetson/src/bpftime-gpu-work/policy
L=$G/logs/dvfs
CELLS=(
  "c01 host on6 2000 3200 1 1000 0 0 1"      # full load
  "c02 host off 2000 3200 1 1000 0 0 1"      # no load (power)
  "c05 host on6 2000 2400 32 1000 0 0 1"     # burst, full load
  "c06 host off 2000 2400 32 1000 0 0 1"     # burst, no load
  "c11 host off 2000 1 1 0 8000000 0 1"      # idle agent: one launch, then 8 s
  "c17 host duty6 2000 3200 1 1000 0 0 1"    # load 5 ms on / 5 ms off
  "c23 host off 30 4000 1 60 0 0 1"          # request gaps about 55-115 us, no load
  "c25 host off 2000 2000 2 1000 0 0 1"      # 2 calls per launch, no load
)
restore() {
  $G/clocks.sh on >/dev/null 2>&1 || $G/clocks.sh on
  $G/clocks.sh check-on && echo "restored jetson_clocks $(date +%T)" || echo "RESTORE FAILED $(date +%T)"
}
trap restore EXIT
trap 'exit 3' INT TERM HUP
for r in $(seq "$1" "$2"); do
  if [ $((r % 2)) = 1 ]; then regimes="off on"; else regimes="on off"; fi
  for k in $regimes; do
    $G/clocks.sh $k || { echo "ABORT: cannot switch to $k $(date +%T)"; exit 2; }
    $G/clocks.sh snapshot $L/clocks-r$r-$k.txt 2>/dev/null || { mkdir -p $L; $G/clocks.sh snapshot $L/clocks-r$r-$k.txt; }
    sleep 5
    mkdir -p $L/$k/r$r
    for c in "${CELLS[@]}"; do
      c=${c%%#*}
      read -r name pr ld d l t j gap gr np <<< "$c"
      order=$(python3 -c "import random,sys; s='P W D AC3 M100 AC2 AC4'.split(); random.Random(sys.argv[1]).shuffle(s); print(*s)" "dvfs-r$r-$k-$name")
      echo "r$r $k $name order: $(echo $order) $(date +%T)"
      for s in $order; do
        CLOCKS=$k $G/run_one.sh $s $pr $ld $d $l $t $j $gap $gr $np $L/$k/r$r/${name}_$s; rc=$?
        [ $rc = 2 ] && { echo "ABORT round $r $k $name $s $(date +%T)"; exit 2; }
      done
    done
    echo "round $r $k done $(date +%T)"
  done
done
