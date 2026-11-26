#!/usr/bin/env bash
# E7a (H7a, nbthread=1) then E7b (H7b, nbthread=4) on the haproxy image. Each session takes the lock itself.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
export PAPER_IMAGE=bpftime-ssl-haproxy:x86-20261126
waitload() { until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done; }
for S in H7a H7b; do
  SID=$S-20261126-try1
  if [ $S = H7b ]; then export PAPER_PINS="0:3500000 1:3500000 2:3500000 3:3500000 4:3500000 6:3500000 7:3500000 8:2500000 9:2500000 10:2500000 11:2500000"; else unset PAPER_PINS; fi
  for i in $(seq 10); do waitload; "$R/harness/session.sh" $SID "$R/plans/$S.txt" > "$R/logs/$SID.out" 2>&1; s=$?
    [ $s -ne 3 ] && ! grep -q "load too high" "$R/logs/$SID.out" && break; rm -rf "$R/raw/$SID"; sleep 60; done
  echo "$S exit $s $(date +%T)"
done
echo CHAIN-DONE
