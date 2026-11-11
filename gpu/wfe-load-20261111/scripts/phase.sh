#!/usr/bin/env bash
# Phase sweep, no load: does the host-call wait of N and W0 depend on where the call falls in the
# watcher's sleep schedule? Spin durations in 100 us steps around 2 ms and 20 ms (~2 s each).
# phase.sh ROUND
set -u
G=/home/jetson/src/bpftime-gpu-work/load; r=$1
mkdir -p $G/logs/phase$r
for d in 2000 2100 2200 2300 2400 2500 2600 2700 2800 2900 3000 \
         20000 20100 20200 20300 20400 20500 20600 20700 20800 20900 21000; do
  [ $d -lt 10000 ] && l=1000 || l=100
  for s in N W0; do
    $G/run_one.sh $s host off $d $l $G/logs/phase$r/off_${d}_host_$s; rc=$?
    [ $rc = 2 ] && { echo "ABORT $d $s"; exit 2; }
  done
done
echo "phase $r done $(date +%T)"
