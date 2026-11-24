#!/usr/bin/env bash
# Follows the night chain: E3 Opc/Dpc (Opc Dpc Dpc Opc). apt-daily timers stopped by the user's decision.
# Opc run ~70 min, Dpc ~50 min, bookends 3 x 15 min -> ~4.7 h; run timeout 7200 s, guard 300 min.
set -uo pipefail
cd $HOME/src/paper-runs-20261124
while pgrep -f chain-20261124a.sh >/dev/null; do sleep 30; done
echo "night chain gone $(date +%T)"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T): $(cat /proc/loadavg)"
E3_RUN_TIMEOUT=7200 E3_GUARD_MIN=300 scripts/run-e3.sh J-E3pc-20261124 Opc Dpc Dpc Opc; echo "E3pc exit=$?"
