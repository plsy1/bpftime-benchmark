#!/bin/bash
# The runs C4's rules name, each in a fresh process, bounded, logged.
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
mkdir -p logs
run() { # name binary args...
  local name=$1; shift
  echo "### $name: $*" | tee "logs/$name.log"
  timeout 600 "$@" >> "logs/$name.log" 2>&1
  local rc=$?
  echo "### exit=$rc" | tee -a "logs/$name.log"
  grep -E '^(RESULT|trials_with_violation|violations by kind|counted)' "logs/$name.log"
}
run r1-proto-128         ./build/c4_proto      --max 128
run r2-proto-48          ./build/c4_proto      --max 48
run r3-ctl1-plainclaim   ./build/c4_plainclaim --max 128
run r4-ctl2-nobound-48   ./build/c4_nobound    --max 48
run r5-ctl3-relaxpub     ./build/c4_relaxpub   --max 128
