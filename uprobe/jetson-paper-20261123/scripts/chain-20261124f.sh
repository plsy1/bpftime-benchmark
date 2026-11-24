#!/usr/bin/env bash
# After the E10-E14 chain: cpuset control pinbench (NUC request), E7c cache/TLB counters, then the second half of E3 Opc/Dpc (run3 Dpc, run4 Opc), interrupted at 05:59 by the chain-b outer timeout.
set -uo pipefail
cd $HOME/src/paper-runs-20261124
while pgrep -f chain-20261124e.sh >/dev/null; do sleep 30; done
echo "E1x chain gone $(date +%T)"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
scripts/run-pinbench-cpuset.sh J-pinbench-cpuset-20261124; echo "cpuset exit=$?"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
PAPER_EXEC_PATH=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  harness/session.sh J-E7c-20261124 plans/E7c.txt 20; echo "E7c exit=$?"
until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 15; done; echo "load ok $(date +%T)"
PART2=1 E3_RUN_TIMEOUT=7200 E3_GUARD_MIN=180 scripts/run-e3.sh J-E3pc2-20261124 Opc Dpc Dpc Opc; echo "E3pc2 exit=$?"
