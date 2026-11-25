#!/usr/bin/env bash
# x86 chain 2026-11-25 night 2: I3 payload sweep, then I9 co-location (policy C). Each session takes the lock itself.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
waitload() { until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done; }
for S in I3 I9; do
  SID=$S-20261125-try1
  for i in $(seq 30); do waitload; "$R/harness/session.sh" $SID "$R/plans/$S.txt" > "$R/logs/$SID.out" 2>&1; s=$?
    [ $s -ne 3 ] && ! grep -q "load too high" "$R/logs/$SID.out" && break; rm -rf "$R/raw/$SID"; sleep 60; done
  echo "$S exit $s $(date +%T)"
done
echo CHAIN-DONE
