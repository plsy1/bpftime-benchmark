#!/usr/bin/env bash
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done
"$R/harness/session.sh" DX1-20261125-try1 "$R/plans/DX1.txt" > "$R/logs/DX1-20261125-try1.out" 2>&1; echo "DX1 exit $?"
