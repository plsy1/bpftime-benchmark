#!/bin/bash
# The frozen C6 runs, each a fresh process under the GPU lock with timeout 900 (run.sh):
# harness checks SD and S (outside the rules table, 10^7 instances each), then the rule runs
# A, B, C, D once each with the rules' stop rule (10^8 instances or 600 s, the binary's defaults).
set -u
D=$(cd "$(dirname "$0")" && pwd)
cd "$D"
./run.sh check-SD-1e7 SD --target 10000000
./run.sh check-S-1e7 S --target 10000000
for v in A B C D; do
  ./run.sh rule-$v $v
done
echo "run_all done $(date -Is)"
