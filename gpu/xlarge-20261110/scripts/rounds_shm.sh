#!/usr/bin/env bash
# Experiment 1: swap the shared-memory segment size. rounds_shm.sh FIRST LAST
# Sets: O official (50 MB default), P official with 128 MB, D dev (128 MB default), E dev with 50 MB.
set -u
S=/home/jetson/src/results/gpu/xlarge-20261110/scripts; X=/home/jetson/src/bpftime-gpu-work/xlarge
V=/home/jetson/src/bpftime-gpu-work/compare
SETS=(O P D E)
view() { case $1 in O|P) echo official;; *) echo candidate;; esac; }
envof() { case $1 in P) echo --env=BPFTIME_SHM_MEMORY_MB=128;; E) echo --env=BPFTIME_SHM_MEMORY_MB=50;; esac; }
for r in $(seq "$1" "$2"); do
  n=${#SETS[@]}; off=$(( (r - 1) % n ))
  for i in $(seq 0 $((n - 1))); do
    s=${SETS[$(( (i + off) % n ))]}; id=$s$r
    python3 $S/gen_x.py $id $(envof $s) >/dev/null || exit 1
    bash $V/run_batch.sh $(view $s) $X/configs/$id.json 600; rc=$?
    [ $rc = 2 ] && { echo "ABORT round $r set $s"; exit 2; }
  done
  echo "round $r done $(date +%T)"
done
