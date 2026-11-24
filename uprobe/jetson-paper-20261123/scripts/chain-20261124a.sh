#!/usr/bin/env bash
# Night chain (Jetson): after the evening chain -> E5-R2 -> E6 -> E8. E3 Opc/Dpc moved to the day (100k calls x ~0.9 ms per checked read/write: run > 3600 s, does not fit before 06:15).
# Must finish before the apt-daily window (06:15 - 20 min); each session's guard checks the timers.
set -uo pipefail
cd $HOME/src/paper-runs-20261124
while pgrep -f chain-20261123b.sh >/dev/null; do sleep 30; done
echo "evening chain gone $(date +%T)"
waitload() { until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T): $(cat /proc/loadavg)"; }
waitload; harness/session.sh J-E5-20261124 plans/E5.txt 15; echo "E5 exit=$?"
waitload; harness/session.sh J-E6-20261124 plans/E6.txt 130; echo "E6 exit=$?"
waitload; harness/session.sh J-E8-20261124 plans/E8.txt 50; echo "E8 exit=$?"
