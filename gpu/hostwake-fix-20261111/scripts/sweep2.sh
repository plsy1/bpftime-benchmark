#!/usr/bin/env bash
# 3 rounds of the gap sweep on dev aa6eb973 (l1 build), each followed by the same gaps without
# bpftime (plain victim, per-launch host time only).
set -u
H=/home/jetson/src/bpftime-gpu-work/hostwake; B=${BUILD:-/home/jetson/src/bpftime-gpu-work/l1/build-cuda}; P=${PREFIX:-}
GAPS="0 50 100 150 200 250 300 400 500 750 1000 2000 5000 10000 20000"
for r in 1 2 3; do
  $H/run_wake.sh $B $H/logs/${P}r$r 200 $GAPS || exit $?
  mkdir -p $H/logs/${P}plain$r
  for g in $GAPS; do timeout 300 $H/victim/vec_add_gap 1 200 1 1 $g > $H/logs/${P}plain$r/gap$g.victim.out 2>&1; done
  echo "round $r done $(date +%T)"
done
