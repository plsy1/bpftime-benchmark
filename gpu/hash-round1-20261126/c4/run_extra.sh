#!/bin/bash
# Runs beyond the rules (reported separately): concurrent reader, keys inserted
# beforehand, max_entries 64, and repeated trials (fresh zeroed memory, a new
# hash seed per trial). Each in a fresh process, bounded, logged.
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
mkdir -p logs
run() { # name binary args...
  local name=$1; shift
  echo "### $name: $*" | tee "logs/$name.log"
  local s=$(date +%s.%N)
  timeout 600 "$@" >> "logs/$name.log" 2>&1
  local rc=$?
  echo "### exit=$rc wall_s=$(echo "$(date +%s.%N) - $s" | bc)" | tee -a "logs/$name.log"
  grep -E '^(RESULT|trials_with_violation|violations by kind|counted|reader: trials)' "logs/$name.log"
}
run r6-proto-128-reader        ./build/c4_proto      --max 128 --reader 1
run r7-ctl3-relaxpub-reader    ./build/c4_relaxpub   --max 128 --reader 1
run r8-proto-128-prefill16     ./build/c4_proto      --max 128 --prefill 16
run r9-proto-64                ./build/c4_proto      --max 64
run t1-proto-128-x1000         ./build/c4_proto      --max 128 --trials 1000
run t2-proto-48-x1000          ./build/c4_proto      --max 48  --trials 1000
run t3-proto-64-x1000          ./build/c4_proto      --max 64  --trials 1000
run t4-proto-128-prefill16-x300 ./build/c4_proto     --max 128 --prefill 16 --trials 300
run t5-proto-128-reader-x300   ./build/c4_proto      --max 128 --reader 1 --trials 300
run t6-relaxpub-128-reader-x300 ./build/c4_relaxpub  --max 128 --reader 1 --trials 300
run t7-relaxpub-128-x1000      ./build/c4_relaxpub   --max 128 --trials 1000
run t8-plainclaim-128-x200     ./build/c4_plainclaim --max 128 --trials 200
run t9-nobound-48-x200         ./build/c4_nobound    --max 48  --trials 200
# Does the "key in two slots" check ever fire? The plain-store control at fuller directories.
run t10-plainclaim-64-x1000    ./build/c4_plainclaim --max 64  --trials 1000 --detail 0
run t11-plainclaim-48-x1000    ./build/c4_plainclaim --max 48  --trials 1000 --detail 0
# Extra control (FAULT 4, not in the rules): plain-store claim and no reservation together.
run t12-plainnobound-128-x200  ./build/c4_plainnobound --max 128 --trials 200 --detail 0
run t13-plainnobound-48-x200   ./build/c4_plainnobound --max 48  --trials 200 --detail 0
# Wait bound 0 (build c4_proto_nowait): exercises the unstable path and "EEXIST then NULL".
run t14-proto-nowait-128-x300  ./build/c4_proto_nowait --max 128 --trials 300
