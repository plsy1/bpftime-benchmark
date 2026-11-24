#!/usr/bin/env bash
# E15 (kernel consumer wakeup batching) then E16 (exact checked runtime Dx), NUC requests 2026-11-24.
set -uo pipefail
cd $HOME/src/paper-runs-20261124
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
harness/session.sh J-E15-20261124 plans/E15.txt 75; echo "E15 exit=$?"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
harness/session.sh J-E16-20261124 plans/E16.txt 90; echo "E16 exit=$?"
