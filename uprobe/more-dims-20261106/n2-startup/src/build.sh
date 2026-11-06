#!/bin/bash
# Builds shortlived, spawnloop and many-M (M probe-able functions) for n2 (start-up) and n4
# (many probes); the BPF objects are output-dims-20261106's.
set -eu
cd "$(dirname "$0")"
gcc-13 -O2 -g -o shortlived shortlived.c
gcc-13 -O2 -g -o spawnloop spawnloop.c
for m in 1 10 100 1000; do
  python3 many.py $m
  gcc-13 -O2 -g -o many-$m many-$m.c
done
ls -1 shortlived spawnloop many-*[0-9]
