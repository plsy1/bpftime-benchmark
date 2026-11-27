#!/usr/bin/env bash
# P10: waits until no session holds the measurement lock, then runs with the usual load wait / retry.
# Usage: run-C2.sh 1|2 [not-before "YYYY-MM-DD HH:MM"]
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; AT=${1:-}
if [ -n "$AT" ]; then t=$(date -d "$AT" +%s); while [ "$(date +%s)" -lt "$t" ]; do sleep 60; done; fi
SID=P10-$(date +%Y%m%d)-try1
until flock -n "$R/.measure.lock" true; do sleep 30; done
waitload() { until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done; }
for i in $(seq 20); do waitload; "$R/harness/session.sh" $SID "$R/plans/P10.txt" > "$R/logs/$SID.out" 2>&1; s=$?
  [ $s -ne 3 ] && ! grep -q "load too high" "$R/logs/$SID.out" && break; rm -rf "$R/raw/$SID"; sleep 60; done
echo "P10 ($SID) exit $s $(date '+%F %T')"
