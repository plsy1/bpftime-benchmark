#!/usr/bin/env bash
# Tonight's x86 chain (2026-11-25): E3 (session, pinned), then E4 full matrix, then E5 full audit. Each step takes the lock itself.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
waitload() { until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done; }
for i in $(seq 30); do waitload; "$R/harness/session.sh" E3-20261125-try1 "$R/plans/E3.txt" > "$R/logs/E3-20261125-try1.out" 2>&1; s=$?
  [ $s -ne 3 ] && ! grep -q "load too high" "$R/logs/E3-20261125-try1.out" && break; rm -rf "$R/raw/E3-20261125-try1"; sleep 60; done
echo "E3 exit $s $(date +%T)"
waitload; "$R/e4/run-e4.sh" E4-20261125-try1 --reps 5 > "$R/logs/E4-20261125-try1.out" 2>&1; echo "E4 exit $? $(date +%T)"
waitload; (cd "$R/e5" && python3 run_e5.py --legs K,O,Ol,Dn,Dnpc,Dxn --scenarios W1,W2,W3,W3c,W5,W6,W6t,W6tc,W7g,W7r,W8a,W8b,W8c,W8d,W12 \
  --iters 500 --timeout 240 --churn-seconds 120 --outdir runs/E5-20261125-try1 --out results/E5-20261125-try1.jsonl) > "$R/logs/E5-20261125-try1.out" 2>&1
echo "E5 exit $? $(date +%T)"; echo CHAIN-DONE
