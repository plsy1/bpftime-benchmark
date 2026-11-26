#!/usr/bin/env bash
# 11-25 after chain a: J1 (refused 3x by the guard's own sha256 load; guard fixed) -> J11. Timers paused (user OK).
cd $HOME/src/paper-runs-20261124
export PAPER_EXEC_PATH=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
waitload(){ ok=0; while [ $ok -lt 3 ]; do if awk '{exit !($1 < 0.15)}' /proc/loadavg; then ok=$((ok+1)); else ok=0; fi; sleep 20; done; echo "load ok $(date +%T) $(cat /proc/loadavg)"; }
for try in 4 5 6; do
  waitload; bash prep/J1-run.sh J-J1-20261125; rc=$?; echo "J1 try=$try exit=$rc $(date +%T)"
  [ $rc -eq 0 ] && break; [ -s raw/J-J1-20261125/rows.jsonl ] && break
  mv raw/J-J1-20261125 raw/J-J1-20261125.refused-$try-$(date +%H%M)
done
for try in 1 2 3 4; do
  waitload; harness/session.sh J-J11-20261125 plans/J11.txt 70; rc=$?; echo "J11 try=$try exit=$rc $(date +%T)"
  [ $rc -eq 0 ] && break; [ -s raw/J-J11-20261125/rows.jsonl ] && break
  mv raw/J-J11-20261125 raw/J-J11-20261125.refused-$try-$(date +%H%M)
done
echo "chain b done $(date +%T)"
