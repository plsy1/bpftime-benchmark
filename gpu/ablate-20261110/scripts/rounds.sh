#!/usr/bin/env bash
# Interleaved ablation rounds: rounds.sh FIRST LAST
# Sets: O official, D dev, L dev with BPFTIME_CUDA_LOAD_SUBSTITUTION=0 (late path),
#       F dev with 32 GPU stack frames, G dev without the probe gate, T dev with official's
#       array-map and ringbuf-full device code. Order rotates every round.
set -u
A=/home/jetson/src/bpftime-gpu-work/ablate; V=/home/jetson/src/bpftime-gpu-work/compare
SETS=(O D L F G T)
view() { case $1 in O) echo official;; D|L) echo candidate;; F) echo frame;; G) echo gate;; T) echo tramp;; esac; }
tree() { [ "$1" = O ] && echo official || echo candidate; }
for r in $(seq "$1" "$2"); do
  n=${#SETS[@]}; off=$(( (r - 1) % n ))
  for i in $(seq 0 $((n - 1))); do
    s=${SETS[$(( (i + off) % n ))]}; id=$s$r
    envarg=""; [ "$s" = L ] && envarg="--env=BPFTIME_CUDA_LOAD_SUBSTITUTION=0"
    for kind in micro examples; do
      python3 $A/gen_ablate.py $id $(tree $s) $kind $envarg >/dev/null || exit 1
      to=900; [ $kind = examples ] && to=300
      bash $V/run_batch.sh $(view $s) $A/configs/$id-$kind.json $to; rc=$?
      [ $rc = 2 ] && { echo "ABORT round $r set $s"; exit 2; }
    done
  done
  echo "round $r done $(date +%T)"
done
