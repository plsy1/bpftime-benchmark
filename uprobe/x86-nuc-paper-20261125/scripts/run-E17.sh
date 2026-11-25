#!/usr/bin/env bash
# E17: O-latest (upstream 70f7bc1f) next to O, K, K_wb32; policy S and M; syscall counts; hook stage.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; H=$R/harness; P=$R/plans
waitload() { until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 5; done; }
for i in 1 2 3; do waitload; "$H/session.sh" "E17-olatest-20261124-try$i" "$P/E17-olatest.txt" > "$R/logs/E17-olatest-20261124-try$i.out" 2>&1; s=$?; echo "E17 try$i exit $s"
  [ $s -eq 0 ] && break; grep -q "load too high" "$R/logs/E17-olatest-20261124-try$i.out" || break; done
echo E17-DONE
