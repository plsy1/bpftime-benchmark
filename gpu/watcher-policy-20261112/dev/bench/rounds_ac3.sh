#!/usr/bin/env bash
# AC3 in dev on the 24-case subset with the ready runner, interleaved, order alternating:
# W dev 46f5a7cb (wake build, WFE), D dev 52126f33 (the worktree's build-cuda, AC3).
# rounds_ac3.sh FIRST LAST
set -u
G=/home/jetson/src/bpftime-gpu-work/sub; V=/home/jetson/src/bpftime-gpu-work/compare
R=/home/jetson/src/results/gpu/watcher-policy-20261112/dev/bench
export RUNNER=$V/runner/run_cuda_bench_ready.py
SETS=(W D)
for r in $(seq "$1" "$2"); do
  n=${#SETS[@]}; off=$(( (r - 1) % n ))
  for i in $(seq 0 $((n - 1))); do
    s=${SETS[$(( (i + off) % n ))]}; id=A$s$r
    case $s in W) b=wake;; D) b=candidate;; esac
    python3 $G/gen_sub.py $R $id >/dev/null || exit 1
    bash $V/run_batch.sh $b $G/configs/$id.json 900; rc=$?
    [ $rc = 2 ] && { echo "ABORT round $r set $s"; exit 2; }
  done
  echo "round $r done $(date +%T)"
done
