#!/usr/bin/env bash
set -u
A=/home/jetson/src/bpftime-gpu-work/ablate; V=/home/jetson/src/bpftime-gpu-work/compare
for s in O D L F G T; do
  case $s in O) v=official; t=official;; D|L) v=candidate; t=candidate;; F) v=frame; t=candidate;; G) v=gate; t=candidate;; T) v=tramp; t=candidate;; esac
  env="--env=BPFTIME_CUDA_KEEP_PTX=1"; [ $s = L ] && env="--env=BPFTIME_CUDA_LOAD_SUBSTITUTION=0,BPFTIME_CUDA_KEEP_PTX=1"
  python3 $A/gen_ablate.py S-$s $t smoke --level=debug $env >/dev/null || exit 1
  bash $V/run_batch.sh $v $A/configs/S-$s-smoke.json 900 || [ $? != 2 ] || exit 2
done
