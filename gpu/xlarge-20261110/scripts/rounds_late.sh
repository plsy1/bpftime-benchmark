#!/usr/bin/env bash
# Experiment 4: launch path together with the device code. Sets: O official, D dev, L dev late path,
# C fgt (official's device code on dev), K fgt with the late path (BPFTIME_CUDA_LOAD_SUBSTITUTION=0).
# rounds_late.sh FIRST LAST
set -u
S=/home/jetson/src/results/gpu/xlarge-20261110/scripts; X=/home/jetson/src/bpftime-gpu-work/xlarge
V=/home/jetson/src/bpftime-gpu-work/compare
SETS=(O D L C K)
view() { case $1 in O) echo official;; D|L) echo candidate;; C|K) echo fgt;; esac; }
envof() { case $1 in L|K) echo --env=BPFTIME_CUDA_LOAD_SUBSTITUTION=0;; esac; }
CASES="Baseline (xlarge)|GPU Array map update (large)|GPU Array map update (xlarge)|GPU Array map update (xlarge_n32)"
for r in $(seq "$1" "$2"); do
  n=${#SETS[@]}; off=$(( (r - 1) % n ))
  for i in $(seq 0 $((n - 1))); do
    s=${SETS[$(( (i + off) % n ))]}; id=K$s$r
    python3 $S/gen_x.py $id "--cases=$CASES" $(envof $s) >/dev/null || exit 1
    bash $V/run_batch.sh $(view $s) $X/configs/$id.json 600; rc=$?
    [ $rc = 2 ] && { echo "ABORT round $r set $s"; exit 2; }
  done
  echo "round $r done $(date +%T)"
done
