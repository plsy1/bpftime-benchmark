#!/usr/bin/env bash
set -uo pipefail
cd $HOME/src/paper-runs-20261124
waitload() { until awk '{exit !($1 < 0.25)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T): $(cat /proc/loadavg)"; }
waitload; harness/session.sh J-E2-20261123 plans/E2.txt 60; echo "E2 exit=$?"
waitload; scripts/run-pinbench-idle.sh J-E2-pinbench-idle-20261123; echo "pinbench exit=$?"
waitload; harness/session.sh J-E1x-20261123 plans/E1x.txt 40; echo "E1x exit=$?"
