#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124
until grep -q CHAIN-DONE $R/logs/chain-E1-E2.out; do sleep 10; done
until awk '{exit !($1 < 0.25)}' /proc/loadavg; do sleep 5; done
$R/harness/session.sh E1-extend-20261123 $R/plans/E1-extend.txt > $R/logs/E1-extend.out 2>&1; echo "E1-extend exit $?"
echo EXT-DONE
