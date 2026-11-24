#!/usr/bin/env bash
# After E7: builds for E10-E14 (idle machine, no measurement running), then E12 -> E13 -> E14 -> E10 -> E11.
# Each run script has its own guard window and per-step deadlines (prep/lib.sh).
set -uo pipefail
cd $HOME/src/paper-runs-20261124
while pgrep -f chain-20261124d.sh >/dev/null; do sleep 30; done
echo "E7 chain gone $(date +%T)"
mkdir -p logs/prep-builds
ok=()
for e in E10 E11 E13 E12; do
  timeout 3600 bash prep/$e-build.sh >logs/prep-builds/$e.log 2>&1; rc=$?
  echo "$e build exit=$rc $(date +%T)"; [ $rc -eq 0 ] && ok+=($e)
done
timeout 300 bash prep/E14-build.sh >logs/prep-builds/E14.log 2>&1 && ok+=(E14); echo "E14 build check exit=$? ; ok: ${ok[*]}"
waitload() { until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T): $(cat /proc/loadavg)"; }
for e in E12 E13 E14 E10 E11; do
  [[ " ${ok[*]} " == *" $e "* ]] || { echo "$e skipped (build failed)"; continue; }
  waitload; bash prep/$e-run.sh J-$e-20261124; echo "$e exit=$?"
done
