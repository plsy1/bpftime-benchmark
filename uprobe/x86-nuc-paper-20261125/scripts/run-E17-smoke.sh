#!/usr/bin/env bash
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; H=$R/harness; P=$R/plans
waitload() { until awk '{exit !($1 < 0.25)}' /proc/loadavg; do sleep 5; done; }
for i in 1 2 3; do waitload; "$H/session.sh" "E17smoke-20261124-try$i" "$P/E17-smoke.txt" > "$R/logs/E17smoke-20261124-try$i.out" 2>&1; s=$?; echo "smoke try$i exit $s"
  [ $s -eq 0 ] && break; grep -q "load too high" "$R/logs/E17smoke-20261124-try$i.out" || break; done
echo SMOKE-DONE
