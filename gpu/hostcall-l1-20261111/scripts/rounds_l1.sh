#!/usr/bin/env bash
# Host-call layer 1 against dev c115b33d. Sets: D dev c115b33d (worktree build-cuda), N dev with
# layer 1 (l1/build-cuda). rounds_l1.sh FIRST LAST [PREFIX] [LEVEL]; order alternates per round.
set -u
L=/home/jetson/src/bpftime-gpu-work/l1; V=/home/jetson/src/bpftime-gpu-work/compare
P=${3:-R}; LEVEL=${4:-info}
SETS=(D N)
view() { case $1 in D) echo candidate;; N) echo l1;; esac; }
for r in $(seq "$1" "$2"); do
  n=${#SETS[@]}; off=$(( (r - 1) % n ))
  for i in $(seq 0 $((n - 1))); do
    s=${SETS[$(( (i + off) % n ))]}; id=$P$s$r
    python3 $L/gen_l1.py $id --level=$LEVEL >/dev/null || exit 1
    bash $V/run_batch.sh $(view $s) $L/configs/$id.json 900; rc=$?
    [ $rc = 2 ] && { echo "ABORT round $r set $s"; exit 2; }
  done
  echo "round $r done $(date +%T)"
done
