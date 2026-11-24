#!/usr/bin/env bash
# After E3 Opc/Dpc: loaded pinbench (x86 PBload structure).
set -uo pipefail
cd $HOME/src/paper-runs-20261124
while pgrep -f chain-20261124b.sh >/dev/null; do sleep 30; done
echo "E3pc chain gone $(date +%T)"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
scripts/run-pinbench-loaded.sh J-E2-pinbench-loaded-20261124; echo "PBload exit=$?"
