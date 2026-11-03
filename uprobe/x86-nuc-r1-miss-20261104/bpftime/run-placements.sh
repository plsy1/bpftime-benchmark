#!/bin/bash
# run-placements.sh BUILD [bpf]: bpftime (r1bt-BUILD), and bpf (r1) when asked,
# under the same CPU placements u0,u1,r0,r1,r2; 5 rounds of 2 s each.
set -u
build=$1; with_bpf=${2:-}
for p in "A 0,2,4,6,1" "B 8,9,10,11,12" "C 8,9,0,2,4" "D 0,2,8,9,10"; do
  name=${p%% *}; cpus=${p#* }
  if [ -n "$with_bpf" ]; then echo "== placement $name ($cpus): bpf"; timeout 60 sudo -n ./r1 2 5 $cpus | grep round; fi
  echo "== placement $name ($cpus): bpftime $build"; timeout 60 ./r1bt-$build 2 5 $cpus
done
