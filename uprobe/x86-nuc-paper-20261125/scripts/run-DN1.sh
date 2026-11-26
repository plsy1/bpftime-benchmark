#!/usr/bin/env bash
# DN1 session with the load wait / retry used for the other sessions.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; SID=DN1-20261126-try1
waitload() { until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done; }
for i in $(seq 10); do waitload; "$R/harness/session.sh" $SID "$R/plans/DN1.txt" > "$R/logs/$SID.out" 2>&1; s=$?
  [ $s -ne 3 ] && ! grep -q "load too high" "$R/logs/$SID.out" && break; rm -rf "$R/raw/$SID"; sleep 60; done
echo "DN1 exit $s $(date +%T)"
