#!/usr/bin/env bash
# ATC week 1 on Orin (user OK 11-25 ~14:55): SMOKE-J4 -> J5 -> J4 -> J1. Window ends before apt-daily 18:13.
# Each step waits for load (3 readings < 0.15, 20 s apart); a refused start (no rows) is retried up to 4 times.
cd $HOME/src/paper-runs-20261124
export PAPER_EXEC_PATH=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
waitload(){ ok=0; while [ $ok -lt 3 ]; do if awk '{exit !($1 < 0.15)}' /proc/loadavg; then ok=$((ok+1)); else ok=0; fi; sleep 20; done; echo "load ok $(date +%T) $(cat /proc/loadavg)"; }
sess(){ # sid plan minutes
  for try in 1 2 3 4; do
    waitload
    harness/session.sh $1 $2 $3; rc=$?
    echo "$1 try=$try exit=$rc $(date +%T)"
    [ $rc -eq 0 ] && return 0
    [ -s raw/$1/rows.jsonl ] && { echo "$1 has rows, not retrying"; return $rc; }
    mv raw/$1 raw/$1.refused-$try-$(date +%H%M)
  done; return 1; }
sess J-SMOKE-J4-20261125 plans/SMOKE-J4.txt 15 || { echo "smoke failed, stop"; exit 1; }
python3 - <<'PY' || { echo "smoke rows bad, stop"; exit 1; }
import json
rows=[json.loads(l) for l in open('raw/J-SMOKE-J4-20261125/rows.jsonl')]
for r in rows: print(r['leg'], r['valid'], r['rps'], r.get('sniff_cpu_s'), r.get('del_over_exp'), r['invalid_reason'])
assert len(rows)==4 and all(r['valid'] for r in rows) and all((r.get('sniff_cpu_s') or 0)>=0.3 for r in rows if r['stock'])
PY
sess J-J5-20261125 plans/J5.txt 25
sess J-J4-20261125 plans/J4.txt 60
waitload
for try in 1 2 3; do
  bash prep/J1-run.sh J-J1-20261125; rc=$?; echo "J1 try=$try exit=$rc $(date +%T)"
  [ $rc -eq 0 ] && break
  [ -s raw/J-J1-20261125/rows.jsonl ] && break
  mv raw/J-J1-20261125 raw/J-J1-20261125.refused-$try-$(date +%H%M); waitload
done
echo "chain a done $(date +%T)"
