#!/usr/bin/env bash
# Interleaved rounds of the short-sleep test: rounds.sh FIRST LAST
# Per round 8 cells, in each the 8 sets in an order rotating with the round:
#   single call: probe host x load on/off x spin 2,000 / 20,000 us (+0-999 us jitter)
#   pure cost:   probe none x load on x spin 2,000 / 20,000 us
#   burst:       probe host, 32 lanes, load on/off, spin 2,000 us
set -u
G=/home/jetson/src/bpftime-gpu-work/sleep
SETS=(P N W S10 S20 S50 S100 M100)
CELLS=("host on 2000 3200 1" "host off 2000 3200 1" "host on 20000 400 1" "host off 20000 400 1"
       "none on 2000 3200 1" "none on 20000 400 1" "host on 2000 2400 32" "host off 2000 2400 32")
n=${#SETS[@]}
for r in $(seq "$1" "$2"); do
  mkdir -p $G/logs/r$r
  for c in "${CELLS[@]}"; do
    set -- $c; pr=$1; ld=$2; d=$3; l=$4; t=$5
    for i in $(seq 0 $((n - 1))); do
      s=${SETS[$(( (i + r) % n ))]}
      $G/run_one.sh $s $pr $ld $d $l $t $G/logs/r$r/${pr}_${ld}_${d}_t${t}_$s; rc=$?
      [ $rc = 2 ] && { echo "ABORT round $r $c $s"; exit 2; }
    done
  done
  echo "round $r done $(date +%T)"
done
