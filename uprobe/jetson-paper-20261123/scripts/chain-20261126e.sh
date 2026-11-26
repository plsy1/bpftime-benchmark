#!/usr/bin/env bash
# 11-26: J13s (PREREG-2b M6 supplement), only after NUC E4f gate passed. Bounded by the caller.
cd $HOME/src/paper-runs-20261124
waitload(){ ok=0; while [ $ok -lt 3 ]; do if awk '{exit !($1 < 0.15)}' /proc/loadavg; then ok=$((ok+1)); else ok=0; fi; sleep 20; done; echo "load ok $(date +%T) $(cat /proc/loadavg)"; }
retry(){ # sid cmd...
  sid=$1; shift
  for try in 1 2 3 4; do
    waitload; "$@"; rc=$?; echo "$sid try=$try exit=$rc $(date +%T)"
    [ $rc -eq 0 ] && return 0
    [ -n "$(ls raw/$sid/*.jsonl 2>/dev/null)" ] && { echo "$sid has rows, not retrying"; return $rc; }
    mv raw/$sid raw/$sid.refused-$try-$(date +%H%M)
  done; return 1; }
sha256sum --check --quiet plans/J13s.sha256 || { echo "J13s plan changed, stop"; exit 1; }
retry J-J13s-20261126 harness/session.sh J-J13s-20261126 plans/J13s.txt 80
echo "chain e done $(date +%T)"
