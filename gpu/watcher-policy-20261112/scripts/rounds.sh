#!/usr/bin/env bash
# Interleaved rounds of the watcher-policy test: rounds.sh FIRST LAST
# Per round 18 cells, in each the 7 sets in an order rotating with the round. Cell fields:
# name probe load spin_us launches threads jitter_us gap_us graph procs
set -u
G=/home/jetson/src/bpftime-gpu-work/policy
SETS=(P W M100 A AC AB ABC)
CELLS=(
  "c01 host on6 2000 3200 1 1000 0 0 1"      # full load
  "c02 host off 2000 3200 1 1000 0 0 1"      # no load (power)
  "c03 host on6 20000 400 1 1000 0 0 1"      # W's worst
  "c04 none on6 2000 3200 1 1000 0 0 1"      # pure cost
  "c05 host on6 2000 2400 32 1000 0 0 1"     # burst, full load
  "c06 host off 2000 2400 32 1000 0 0 1"     # burst, no load
  "c07 host on3 2000 3200 1 1000 0 0 1"      # partial load
  "c08 host on3 2000 2400 32 1000 0 0 1"     # burst, partial load
  "c09 host off 2000 400 1 1000 18000 0 1"   # low GPU duty (~12%), no load
  "c10 host on6 2000 400 1 1000 18000 0 1"   # low GPU duty, full load
  "c11 host off 2000 1 1 0 8000000 0 1"      # idle agent: one launch, then 8 s
  "c12 host on6 2000 3200 1 1000 0 0 2"      # two processes, full load
  "c13 host off 2000 3200 1 1000 0 0 2"      # two processes, no load
  "c14 host off 200 4000 1 200 0 0 1"        # short kernels, no load
  "c15 host on6 200 4000 1 200 0 0 1"        # short kernels, full load
  "c16 host on3 2000 3200 1 1000 0 0 2"      # two processes, partial load
  "c17 host duty6 2000 3200 1 1000 0 0 1"    # load 5 ms on / 5 ms off
  "c18 host off 2000 3200 1 1000 0 1 1"      # CUDA graph launches (not noted)
)
n=${#SETS[@]}
for r in $(seq "$1" "$2"); do
  mkdir -p $G/logs/r$r
  for c in "${CELLS[@]}"; do
    c=${c%%#*}
    read -r name pr ld d l t j gap gr np <<< "$c"
    for i in $(seq 0 $((n - 1))); do
      s=${SETS[$(( (i + r) % n ))]}
      $G/run_one.sh $s $pr $ld $d $l $t $j $gap $gr $np $G/logs/r$r/${name}_$s; rc=$?
      [ $rc = 2 ] && { echo "ABORT round $r $name $s $(date +%T)"; exit 2; }
    done
  done
  echo "round $r done $(date +%T)"
done
