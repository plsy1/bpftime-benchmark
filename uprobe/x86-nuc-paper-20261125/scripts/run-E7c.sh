#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124; H=$R/harness; P=$R/plans
waitload() { until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 5; done; }
for i in 1 2 3; do waitload; "$H/session.sh" "E7c-cache-20261124-try$i" "$P/E7c-cache.txt" > "$R/logs/E7c-cache-20261124-try$i.out" 2>&1; s=$?; echo "E7c try$i exit $s"
  [ $s -eq 0 ] && break; grep -q "load too high" "$R/logs/E7c-cache-20261124-try$i.out" || break; done
echo E7C-DONE
