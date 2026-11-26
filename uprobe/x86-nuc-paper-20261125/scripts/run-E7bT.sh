#!/usr/bin/env bash
# E7b tool mode (plans/E7bT.txt, or plans/E7bT-smoke.txt with "smoke") with the load wait / retry of run-DN1.sh.
# Image and CPU pins as H7b (scripts/chain-H7.sh). Usage: run-E7bT.sh [smoke] [SID]
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
export PAPER_IMAGE=bpftime-ssl-haproxy:x86-20261126
export PAPER_PINS="0:3500000 1:3500000 2:3500000 3:3500000 4:3500000 6:3500000 7:3500000 8:2500000 9:2500000 10:2500000 11:2500000"
if [ "${1:-}" = smoke ]; then P=E7bT-smoke; else P=E7bT; fi
SID=${2:-$P-$(date +%Y%m%d)-try1}
waitload() { until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done; }
for i in $(seq 10); do waitload; "$R/harness/session.sh" $SID "$R/plans/$P.txt" > "$R/logs/$SID.out" 2>&1; s=$?
  [ $s -ne 3 ] && ! grep -q "load too high" "$R/logs/$SID.out" && break; rm -rf "$R/raw/$SID"; sleep 60; done
echo "$P exit $s $(date +%T)"
