#!/usr/bin/env bash
# V-n2 (PREREG-2c): wait until C2 night 2 has finished (its runner prints "C2 night 2 ... exit"), never before it.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
until ls "$R"/raw/C2-n2-2026112*/log/exit-status.txt >/dev/null 2>&1 && ! pgrep -f 'run-C2.sh 2' >/dev/null; do sleep 60; done
sleep 120
"$R/scripts/run-V.sh" V-n2
