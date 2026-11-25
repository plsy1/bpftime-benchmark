#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124
for i in 1 2 3; do
  until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 5; done
  $R/harness/session.sh E1-extend-20261123-try$i $R/plans/E1-extend.txt > $R/logs/E1-extend-try$i.out 2>&1; s=$?
  echo "try$i exit $s"
  [ $s -eq 0 ] && break
  grep -q "load too high" $R/logs/E1-extend-try$i.out || break
done
echo EXT-DONE
