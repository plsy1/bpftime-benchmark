#!/usr/bin/env bash
# Interleaved rounds of the WFE-under-load test: rounds.sh FIRST LAST
# Per round, 8 cells (load on/off x spin 2,000/20,000 us x probe host/none), in each cell the
# sets P N W W0 in an order rotating with the round. ~8 s timed window per run.
set -u
G=/home/jetson/src/bpftime-gpu-work/load
SETS=(P N W W0)
for r in $(seq "$1" "$2"); do
  mkdir -p $G/logs/r$r
  for ld in on off; do for d in 2000 20000; do for pr in host none; do
    [ $d = 2000 ] && l=4000 || l=400
    for i in 0 1 2 3; do
      s=${SETS[$(( (i + r) % 4 ))]}
      $G/run_one.sh $s $pr $ld $d $l $G/logs/r$r/${ld}_${d}_${pr}_$s; rc=$?
      [ $rc = 2 ] && { echo "ABORT round $r $ld $d $pr $s"; exit 2; }
    done
  done; done; done
  echo "round $r done $(date +%T)"
done
