#!/usr/bin/env bash
# Evening chain (Jetson): E2s -> E3 (O D D O) -> E4. Starts after the apt-daily window (18:27 + 20 min).
set -uo pipefail
cd $HOME/src/paper-runs-20261124
until [ "$(date +%H%M)" -ge 1850 ]; do sleep 30; done
waitload() { until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T): $(cat /proc/loadavg)"; }
waitload; harness/session.sh J-E2s-20261123 plans/E2s.txt 15; echo "E2s exit=$?"
waitload; scripts/run-e3.sh J-E3-20261123 O D D O; echo "E3 exit=$?"
waitload; harness/session.sh J-E4-20261123 plans/E4.txt 150; echo "E4 exit=$?"
