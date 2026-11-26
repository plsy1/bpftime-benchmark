#!/usr/bin/env bash
# 11-25 evening, timers paused (user OK): wait for the J7 builds -> SMOKE-J7 -> J8 -> J6 -> J2 -> J13, back to back.
cd $HOME/src/paper-runs-20261124
export PAPER_EXEC_PATH=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
waitload(){ ok=0; while [ $ok -lt 3 ]; do if awk '{exit !($1 < 0.15)}' /proc/loadavg; then ok=$((ok+1)); else ok=0; fi; sleep 20; done; echo "load ok $(date +%T) $(cat /proc/loadavg)"; }
retry(){ # sid cmd...
  sid=$1; shift
  for try in 1 2 3 4; do
    waitload; "$@"; rc=$?; echo "$sid try=$try exit=$rc $(date +%T)"
    [ $rc -eq 0 ] && return 0
    [ -n "$(ls raw/$sid/*.jsonl 2>/dev/null)" ] && { echo "$sid has rows, not retrying"; return $rc; }
    mv raw/$sid raw/$sid.refused-$try-$(date +%H%M)
  done; return 1; }
until grep -q "done$" logs/build-J7-20261125.console; do sleep 30; done
grep "rc=" logs/build-J7-20261125.console
[ "$(grep -c 'rc=0' logs/build-J7-20261125.console)" -eq 6 ] || { echo "J7 build failed, stop"; exit 1; }
retry J-SMOKE-J7-20261125 harness/session.sh J-SMOKE-J7-20261125 plans/SMOKE-J7.txt 15 || { echo "smoke failed, stop"; exit 1; }
python3 - <<'PY' || { echo "smoke rows bad, stop"; exit 1; }
import json
rows=[json.loads(l) for l in open('raw/J-SMOKE-J7-20261125/rows.jsonl')]
for r in rows: print(r['leg'], r['valid'], r['rps'], r.get('del_over_exp'), r.get('sniff_cpu_ns'), r.get('nginx_cpu_ns'), r['invalid_reason'])
assert len(rows)==7 and all(r['valid'] for r in rows) and all(r.get('sniff_cpu_ns') for r in rows)
PY
retry J-J8-20261125 harness/session.sh J-J8-20261125 plans/J8.txt 150
retry J-J6-20261125 bash prep/J6-run.sh J-J6-20261125
retry J-J2-20261125 bash prep/J2-run.sh J-J2-20261125
retry J-J13-20261125 harness/session.sh J-J13-20261125 plans/J13.txt 230
echo "chain c done $(date +%T)"
