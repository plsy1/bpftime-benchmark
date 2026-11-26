#!/usr/bin/env bash
# N2 session with the load wait / retry used for the other sessions. CPUs 0-7 pinned at 3.5 GHz (workloads), 8 at 2.5 GHz (tools).
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; SID=N2-20261126-try1
export PAPER_PINS="0:3500000 1:3500000 2:3500000 3:3500000 4:3500000 5:3500000 6:3500000 7:3500000 8:2500000"
waitload() { until awk '{exit !($1 < 0.20)}' /proc/loadavg; do sleep 5; done; }
for i in $(seq 20); do waitload; "$R/harness/session.sh" $SID "$R/plans/N2.txt" > "$R/logs/$SID.out" 2>&1; s=$?
  [ $s -ne 3 ] && ! grep -q "load too high" "$R/logs/$SID.out" && break; rm -rf "$R/raw/$SID"; sleep 60; done
echo "N2 exit $s $(date +%T)"
