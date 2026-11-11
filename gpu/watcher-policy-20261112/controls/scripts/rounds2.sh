#!/usr/bin/env bash
# Control rounds (attribution of AC's gain): rounds2.sh FIRST LAST
# Sets P M100 A A0 AC AC0 in a shuffled order per round and cell (seeded, reproducible).
# Cells as in rounds.sh: name probe load spin_us launches threads jitter_us gap_us graph procs
set -u
G=/home/jetson/src/bpftime-gpu-work/policy
L=$G/logs/ctl
CELLS=(
  "c01 host on6 2000 3200 1 1000 0 0 1"      # full load
  "c02 host off 2000 3200 1 1000 0 0 1"      # no load (power)
  "c10 host on6 2000 400 1 1000 18000 0 1"   # low GPU duty, full load
  "c15 host on6 200 4000 1 200 0 0 1"        # short kernels, full load
  "c17 host duty6 2000 3200 1 1000 0 0 1"    # load 5 ms on / 5 ms off
)
for r in $(seq "$1" "$2"); do
  mkdir -p $L/r$r
  for c in "${CELLS[@]}"; do
    c=${c%%#*}
    read -r name pr ld d l t j gap gr np <<< "$c"
    order=$(python3 -c "import random,sys; s='P M100 A A0 AC AC0'.split(); random.Random(sys.argv[1]).shuffle(s); print(*s)" "r$r-$name")
    echo "r$r $name order: $(echo $order)"
    for s in $order; do
      $G/run_one.sh $s $pr $ld $d $l $t $j $gap $gr $np $L/r$r/${name}_$s; rc=$?
      [ $rc = 2 ] && { echo "ABORT round $r $name $s $(date +%T)"; exit 2; }
    done
  done
  echo "round $r done $(date +%T)"
done
