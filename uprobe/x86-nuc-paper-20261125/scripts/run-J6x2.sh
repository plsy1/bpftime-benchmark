#!/usr/bin/env bash
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done
for i in $(seq 30); do "$R/harness/session.sh" J6x2-20261125-try1 "$R/plans/J6x2.txt" > "$R/logs/J6x2-20261125-try1.out" 2>&1; s=$?; [ $s -ne 3 ] && break; rm -rf "$R/raw/J6x2-20261125-try1"; sleep 60; done; echo "J6x2 exit $s"
