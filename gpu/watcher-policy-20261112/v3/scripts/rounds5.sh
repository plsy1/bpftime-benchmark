#!/usr/bin/env bash
# v3 rounds (window rules): rounds5.sh FIRST LAST. Sets P M100 AC AC2 AC3 AC4 AC5 ACn in a shuffled
# order per round and cell (seeded, reproducible). Cells: name probe load spin_us launches threads
# jitter_us gap_us graph procs. Single-lane cells with more than 4095 launches record the first
# 4095 calls' latency (the probe keeps 4096 records).
set -u
G=/home/jetson/src/bpftime-gpu-work/policy
L=$G/logs/v3
CELLS=(
  "c31 host off 1 30000 1 0 0 0 1"           # steady request gaps under 50 us, no load
  "c32 host on6 1 30000 1 0 0 0 1"           # steady request gaps under 50 us, full load
  "c23 host off 30 4000 1 60 0 0 1"          # request gaps about 55-115 us, no load
  "c24 host on6 30 4000 1 60 0 0 1"          # request gaps about 55-115 us, full load
  "c21 host off 130 4000 1 140 0 0 1"        # request gaps about 155-295 us, no load
  "c25 host off 2000 2000 2 1000 0 0 1"      # 2 calls per launch, no load
  "c26 host off 2000 2000 4 1000 0 0 1"      # 4 calls per launch, no load
  "c27 host off 2000 2000 8 1000 0 0 1"      # 8 calls per launch, no load
  "c28 host on6 2000 2000 4 1000 0 0 1"      # 4 calls per launch, full load
  "c05 host on6 2000 2400 32 1000 0 0 1"     # burst, full load
  "c06 host off 2000 2400 32 1000 0 0 1"     # burst, no load
  "c01 host on6 2000 3200 1 1000 0 0 1"      # full load
  "c02 host off 2000 3200 1 1000 0 0 1"      # no load (power)
  "c15 host on6 200 4000 1 200 0 0 1"        # short kernels, full load
)
for r in $(seq "$1" "$2"); do
  mkdir -p $L/r$r
  for c in "${CELLS[@]}"; do
    c=${c%%#*}
    read -r name pr ld d l t j gap gr np <<< "$c"
    order=$(python3 -c "import random,sys; s='P M100 AC AC2 AC3 AC4 AC5 ACn'.split(); random.Random(sys.argv[1]).shuffle(s); print(*s)" "v3-r$r-$name")
    echo "r$r $name order: $(echo $order) $(date +%T)"
    for s in $order; do
      $G/run_one.sh $s $pr $ld $d $l $t $j $gap $gr $np $L/r$r/${name}_$s; rc=$?
      [ $rc = 2 ] && { echo "ABORT round $r $name $s $(date +%T)"; exit 2; }
    done
  done
  echo "round $r done $(date +%T)"
done
