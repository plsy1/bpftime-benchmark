#!/usr/bin/env bash
# Reference rounds on the 24-case subset with the ready-waiting runner, interleaved, order rotating
# per round: O official 4708f409 (the official baseline), D dev c115b33d (before layer 1), N dev
# aa6eb973 (layer 1 items 1-2). rounds_base.sh FIRST LAST
set -u
G=/home/jetson/src/bpftime-gpu-work/sub; V=/home/jetson/src/bpftime-gpu-work/compare
R=/home/jetson/src/results/gpu/baseline-sub-20261111
export RUNNER=$V/runner/run_cuda_bench_ready.py
SETS=(O D N)
for r in $(seq "$1" "$2"); do
  n=${#SETS[@]}; off=$(( (r - 1) % n ))
  for i in $(seq 0 $((n - 1))); do
    s=${SETS[$(( (i + off) % n ))]}; id=B$s$r
    case $s in O) b=official; t=--tree=official;; D) b=candidate; t=--tree=candidate;; N) b=l1; t=--tree=candidate;; esac
    python3 $G/gen_sub.py $R $id $t >/dev/null || exit 1
    bash $V/run_batch.sh $b $G/configs/$id.json 900; rc=$?
    [ $rc = 2 ] && { echo "ABORT round $r set $s"; exit 2; }
  done
  echo "round $r done $(date +%T)"
done
