#!/usr/bin/env bash
# Experiment 2: move the 1513 counter inside its page. BPFTIME_MAP_GPU_THREAD_COUNT resizes the
# GPU ringbuf 'events' (allocated before the counter; on dev also the 1512 buffer), so the
# counter's segment offset shifts by one ringbuf entry per thread. rounds_place.sh REPS N1 N2
# Sets O official, D dev; order alternates per step. Cases: update large and xlarge.
set -u
S=/home/jetson/src/results/gpu/xlarge-20261110/scripts; X=/home/jetson/src/bpftime-gpu-work/xlarge
V=/home/jetson/src/bpftime-gpu-work/compare
for rep in $(seq 1 "$1"); do
  for n in $(seq "$2" "$3"); do
    if [ $(( (n + rep) % 2 )) = 0 ]; then order="O D"; else order="D O"; fi
    for s in $order; do
      id=$s-t$n-r$rep; view=candidate; [ $s = O ] && view=official
      python3 $S/gen_x.py $id --env=BPFTIME_MAP_GPU_THREAD_COUNT=$n \
        "--cases=GPU Array map update (large)|GPU Array map update (xlarge)" >/dev/null || exit 1
      bash $V/run_batch.sh $view $X/configs/$id.json 300; rc=$?
      [ $rc = 2 ] && { echo "ABORT $id"; exit 2; }
    done
  done
  echo "rep $rep done $(date +%T)"
done
