#!/usr/bin/env bash
# The watcher change on the 24-case subset with the ready runner, interleaved, order alternating:
# N dev aa6eb973 (l1 build), W the watcher change (wake build). rounds_wake.sh FIRST LAST
set -u
G=/home/jetson/src/bpftime-gpu-work/sub; V=/home/jetson/src/bpftime-gpu-work/compare
R=/home/jetson/src/results/gpu/hostwake-fix-20261111
export RUNNER=$V/runner/run_cuda_bench_ready.py
SETS=(N W)
for r in $(seq "$1" "$2"); do
  n=${#SETS[@]}; off=$(( (r - 1) % n ))
  for i in $(seq 0 $((n - 1))); do
    s=${SETS[$(( (i + off) % n ))]}; id=${P:-R}$s$r
    case $s in N) b=l1;; W) b=wake;; esac
    python3 $G/gen_sub.py $R $id >/dev/null || exit 1
    bash $V/run_batch.sh $b $G/configs/$id.json 900; rc=$?
    [ $rc = 2 ] && { echo "ABORT round $r set $s"; exit 2; }
  done
  echo "round $r done $(date +%T)"
done
