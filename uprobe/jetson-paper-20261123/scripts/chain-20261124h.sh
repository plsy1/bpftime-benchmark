#!/usr/bin/env bash
# Second retry: the guard also refuses when a timer fired within the last 20 min (man-db fired 09:03:53), so start after 09:24.
# Wait until man-db has fired (its next elapse moves to tomorrow), then E14, then E3 Opc/Dpc second half.
set -uo pipefail
cd $HOME/src/paper-runs-20261124
until [ "$(date +%s)" -gt "$(date -d '2026-11-24 09:24:30' +%s)" ] && ! systemctl is-active -q man-db.service; do sleep 30; done
echo "man-db passed $(date +%T): $(systemctl show man-db.timer -p NextElapseUSecRealtime)"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
bash prep/E14-run.sh J-E14-20261124; echo "E14 exit=$?"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
PART2=1 E3_RUN_TIMEOUT=7200 E3_GUARD_MIN=180 scripts/run-e3.sh J-E3pc2-20261124 Opc Dpc Dpc Opc; echo "E3pc2 exit=$?"
