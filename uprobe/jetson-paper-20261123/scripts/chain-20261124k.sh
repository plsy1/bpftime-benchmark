#!/bin/bash
# Re-run E18a (round 0) after the 16:25 guard refusal (load 0.42); retry on guard refusal.
cd ~/src/paper-runs-20261124
export PAPER_EXEC_PATH=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
waitload(){ ok=0; while [ $ok -lt 3 ]; do if awk '{exit !($1 < 0.15)}' /proc/loadavg; then ok=$((ok+1)); else ok=0; fi; sleep 20; done; echo "load ok $(date +%T) $(cat /proc/loadavg)"; }
for try in 1 2 3 4 5 6; do
  waitload
  harness/session.sh J-E18a-20261124 plans/E18a.txt 60; rc=$?
  echo "E18a try=$try exit=$rc"
  if [ $rc -eq 0 ]; then break; fi
  if [ -s raw/J-E18a-20261124/rows.jsonl ]; then echo "rows exist, not retrying"; break; fi
  mv raw/J-E18a-20261124 raw/J-E18a-20261124.refused-$try-$(date +%H%M)
done
