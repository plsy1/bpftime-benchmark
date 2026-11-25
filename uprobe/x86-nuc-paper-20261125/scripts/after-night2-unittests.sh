#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124
until grep -q CHAIN2-DONE $R/logs/chain-night2.out; do sleep 30; done
$R/scripts/run-unittests.sh > $R/logs/unittests2.out 2>&1; echo "unittests2 $(cat $R/raw/E0-x86-unittests/summary.txt)"
echo UT2-DONE
