#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124
waitload() { until awk '{exit !($1 < 0.25)}' /proc/loadavg; do sleep 5; done; }
waitload; $R/harness/session.sh E1-pilot-20261123 $R/plans/E1-pilot.txt > $R/logs/E1.out 2>&1; echo "E1 exit $?"
waitload; $R/harness/session.sh E2-affinity-20261123 $R/plans/E2-affinity.txt > $R/logs/E2.out 2>&1; echo "E2 exit $?"
echo CHAIN-DONE
