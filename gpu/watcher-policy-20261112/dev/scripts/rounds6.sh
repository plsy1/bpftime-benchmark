#!/usr/bin/env bash
# dev AC3 rounds: rounds6.sh FIRST LAST. Sets P, W (dev 46f5a7cb, WFE), D (dev with AC3) and AC3 (the
# experimental build, V2=1 WIN_MODE=3) in a shuffled order per round and cell (seeded). Cells: name
# probe load spin_us launches threads jitter_us gap_us graph procs.
set -u
G=/home/jetson/src/bpftime-gpu-work/policy
L=$G/logs/dev
CELLS=(
  "c01 host on6 2000 3200 1 1000 0 0 1"      # full load
  "c02 host off 2000 3200 1 1000 0 0 1"      # no load (power)
  "c05 host on6 2000 2400 32 1000 0 0 1"     # burst, full load
  "c06 host off 2000 2400 32 1000 0 0 1"     # burst, no load
  "c11 host off 2000 1 1 0 8000000 0 1"      # idle agent: one launch, then 8 s
  "c15 host on6 200 4000 1 200 0 0 1"        # short kernels, full load
  "c17 host duty6 2000 3200 1 1000 0 0 1"    # load 5 ms on / 5 ms off
  "c18 host off 2000 3200 1 1000 0 1 1"      # CUDA graph launches (not noted)
  "c21 host off 130 4000 1 140 0 0 1"        # request gaps about 155-295 us, no load
  "c23 host off 30 4000 1 60 0 0 1"          # request gaps about 55-115 us, no load
  "c24 host on6 30 4000 1 60 0 0 1"          # request gaps about 55-115 us, full load
  "c25 host off 2000 2000 2 1000 0 0 1"      # 2 calls per launch, no load
  "c28 host on6 2000 2000 4 1000 0 0 1"      # 4 calls per launch, full load
  "c29 host off 2000 2000 1 1000 0 2 1"      # graph and plain launches alternate
  "c30 host off 2000 2000 1 1000 0 3 1"      # graphs, then plain launches (unlatch)
  "c31 host off 1 30000 1 0 0 0 1"           # steady request gaps under 50 us, no load
  "c32 host on6 1 30000 1 0 0 0 1"           # steady request gaps under 50 us, full load
)
for r in $(seq "$1" "$2"); do
  mkdir -p $L/r$r
  for c in "${CELLS[@]}"; do
    c=${c%%#*}
    read -r name pr ld d l t j gap gr np <<< "$c"
    order=$(python3 -c "import random,sys; s='P W D AC3'.split(); random.Random(sys.argv[1]).shuffle(s); print(*s)" "dev-r$r-$name")
    echo "r$r $name order: $(echo $order) $(date +%T)"
    for s in $order; do
      $G/run_one.sh $s $pr $ld $d $l $t $j $gap $gr $np $L/r$r/${name}_$s; rc=$?
      [ $rc = 2 ] && { echo "ABORT round $r $name $s $(date +%T)"; exit 2; }
    done
  done
  echo "round $r done $(date +%T)"
done
