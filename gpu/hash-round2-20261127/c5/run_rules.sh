#!/bin/bash
# C5's runs and controls (RULES-round2.md), each a fresh process under the GPU
# lock, bounded, logged. Rule run = one trial with the default seed and full
# detail; then the trial batch with another seed (fresh memory, new hash seed
# per trial).
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
mkdir -p logs
LOCK=/tmp/claude-2002/-home-jetson-src/548cc3e6-4b28-4a87-b830-6e00c1ee829e/scratchpad/gpu.lock
run() { # name binary args...
  local name=$1; shift
  echo "### $name: $*" | tee "logs/$name.log"
  local s=$(date +%s.%N)
  timeout 2400 flock "$LOCK" "$@" >> "logs/$name.log" 2>&1
  local rc=$?
  echo "### exit=$rc wall_s=$(echo "$(date +%s.%N) - $s" | bc)" | tee -a "logs/$name.log"
  grep -E '^(RESULT|trials_with_violation|violations by kind|counted|refused slots|per trial|reader: trials)' "logs/$name.log"
}
B=./build
SEED=0x5EED20261126
# protocol v2
run a0-proto-64k-128        $B/c5_proto --max 128
run a1-proto-64k-128-x1000  $B/c5_proto --max 128 --trials 1000 --seed $SEED --detail 0
run b0-proto-64k-64         $B/c5_proto --max 64
run b1-proto-64k-64-x1000   $B/c5_proto --max 64 --trials 1000 --seed $SEED --detail 0
run c0-proto-64k-48         $B/c5_proto --max 48
run c1-proto-64k-48-x1000   $B/c5_proto --max 48 --trials 1000 --seed $SEED --detail 0
run d0-proto-64k-128-pre16  $B/c5_proto --max 128 --prefill 16
run d1-proto-64k-128-pre16-x200 $B/c5_proto --max 128 --prefill 16 --trials 200 --seed $SEED --detail 0
run e0-proto-perthread-1024 $B/c5_proto --keys 0 --max 1024
run e1-proto-perthread-1024-x20 $B/c5_proto --keys 0 --max 1024 --trials 20 --seed $SEED --detail 0
run f0-proto-perthread-48   $B/c5_proto --keys 0 --max 48
run f1-proto-perthread-48-x20 $B/c5_proto --keys 0 --max 48 --trials 20 --seed $SEED --detail 0
run g0-nowait-64k-128       $B/c5_proto_nowait --max 128
run g1-nowait-64k-128-x300  $B/c5_proto_nowait --max 128 --trials 300 --seed $SEED --detail 0
run h0-proto-64k-128-reader $B/c5_proto --max 128 --reader 1
run h1-proto-64k-128-reader-x300 $B/c5_proto --max 128 --reader 1 --trials 300 --seed $SEED --detail 0
# controls (each must fail its named check)
run k1-ctl1-plainclaim-128      $B/c5_ctl1-plainclaim --max 128 --trials 201 --detail 1
run k2-ctl2-nocheck-48          $B/c5_ctl2-nocheck --max 48 --trials 201 --detail 1
run k5-ctl5-noreread-64         $B/c5_ctl5-noreread --max 64 --trials 201 --detail 1
run k6-ctl6-roundone-128        $B/c5_ctl6-roundone --max 128 --trials 201 --detail 1
echo "### all done"
