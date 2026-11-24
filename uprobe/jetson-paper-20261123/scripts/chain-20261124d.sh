#!/usr/bin/env bash
# After the loaded pinbench: E7 counters (shared run_paper.py v2 274419bb, perf via host PATH).
set -uo pipefail
cd $HOME/src/paper-runs-20261124
while pgrep -f chain-20261124c.sh >/dev/null; do sleep 30; done
echo "PBload chain gone $(date +%T)"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
PAPER_EXEC_PATH=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  harness/session.sh J-E7-20261124 plans/E7.txt 60; echo "E7 exit=$?"
