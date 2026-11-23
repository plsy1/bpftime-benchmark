#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124
until grep -q UT2-DONE $R/logs/after-night2-ut.out 2>/dev/null; do sleep 30; done
for i in 1 2 3; do until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 5; done
  $R/churn/run-e11.sh E11-churn-20261123-try$i > $R/logs/E11-try$i.out 2>&1; s=$?; echo "E11 try$i exit $s"; [ $s -eq 0 ] && break
  grep -q "load too high" $R/logs/E11-try$i.out || break; done
echo E11-DONE
