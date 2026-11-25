#!/usr/bin/env bash
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done
"$R/harness/session.sh" J6x-20261125-try1 "$R/plans/J6x.txt" > "$R/logs/J6x-20261125-try1.out" 2>&1; echo "J6x exit $?"
