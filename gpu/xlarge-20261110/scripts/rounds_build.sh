#!/usr/bin/env bash
# Experiment 3: O official, D dev, R official with dev's CUDA registration (oreg),
# C dev with official's device code (frame+gate+tramp, fgt). rounds_build.sh FIRST LAST
# Cases: update large/xlarge, the xlarge grid with N=32, baselines.
set -u
S=/home/jetson/src/results/gpu/xlarge-20261110/scripts; X=/home/jetson/src/bpftime-gpu-work/xlarge
V=/home/jetson/src/bpftime-gpu-work/compare
SETS=(O D R C)
view() { case $1 in O) echo official;; D) echo candidate;; R) echo oreg;; C) echo fgt;; esac; }
CASES="Baseline (large)|Baseline (xlarge)|Baseline (xlarge_n32)|GPU Array map update (large)|GPU Array map update (xlarge)|GPU Array map update (xlarge_n32)"
for r in $(seq "$1" "$2"); do
  n=${#SETS[@]}; off=$(( (r - 1) % n ))
  for i in $(seq 0 $((n - 1))); do
    s=${SETS[$(( (i + off) % n ))]}; id=B$s$r
    python3 $S/gen_x.py $id "--cases=$CASES" >/dev/null || exit 1
    bash $V/run_batch.sh $(view $s) $X/configs/$id.json 600; rc=$?
    [ $rc = 2 ] && { echo "ABORT round $r set $s"; exit 2; }
  done
  echo "round $r done $(date +%T)"
done
