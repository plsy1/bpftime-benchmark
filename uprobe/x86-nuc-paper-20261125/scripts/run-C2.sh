#!/usr/bin/env bash
# C2 night N (1 or 2): waits until no session holds the measurement lock, then runs with the usual load wait / retry.
# Usage: run-C2.sh 1|2 [not-before "YYYY-MM-DD HH:MM"]
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; N=${1:?night}; AT=${2:-}
if [ -n "$AT" ]; then t=$(date -d "$AT" +%s); while [ "$(date +%s)" -lt "$t" ]; do sleep 60; done; fi
SID=C2-n$N-$(date +%Y%m%d)
until flock -n "$R/.measure.lock" true; do sleep 30; done
waitload() { until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done; }
for i in $(seq 20); do waitload; "$R/harness/session.sh" $SID "$R/plans/C2-n$N.txt" > "$R/logs/$SID.out" 2>&1; s=$?
  [ $s -ne 3 ] && ! grep -q "load too high" "$R/logs/$SID.out" && break; rm -rf "$R/raw/$SID"; sleep 60; done
echo "C2 night $N ($SID) exit $s $(date '+%F %T')"
