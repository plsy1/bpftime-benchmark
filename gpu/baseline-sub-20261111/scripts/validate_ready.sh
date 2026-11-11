#!/usr/bin/env bash
# Upstream runner (fixed 10 s per probed case) vs the ready-waiting copy, both builds, the 24-case
# subset: validate_ready.sh ROUND. Sets: Do/Dr = dev c115b33d with old/ready runner, No/Nr = dev
# aa6eb973 (l1 build).
set -u
G=/home/jetson/src/bpftime-gpu-work/sub; V=/home/jetson/src/bpftime-gpu-work/compare
R=/home/jetson/src/results/gpu/runner-ready-20261111; r=$1
for s in Do Dr Nr No; do
  id=V$s$r
  python3 $G/gen_sub.py $R $id >/dev/null || exit 1
  case $s in D*) b=candidate;; N*) b=l1;; esac
  case $s in *r) export RUNNER=$V/runner/run_cuda_bench_ready.py;; *) unset RUNNER;; esac
  bash $V/run_batch.sh $b $G/configs/$id.json 900; rc=$?
  [ $rc = 2 ] && { echo "ABORT $id"; exit 2; }
done
