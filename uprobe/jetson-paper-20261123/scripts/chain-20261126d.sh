#!/usr/bin/env bash
# 11-26 early: J6 v2 (NUC pinbench-ws v2, handoff f4e04b88) under the guard, then build Dn730f (no measurement; waits for
# NUC's x86 E4 recheck and PREREG-2b before any J13 supplement).
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
retry J-J6v2-20261126 bash prep/J6v2-run.sh J-J6v2-20261126
R=$HOME/src/paper-runs-20261124; P=$HOME/src/bpftime-paper
chmod u+w $R/builds
echo "=== $(date +%T) Dn730f"
$R/scripts/build-arm2.sh Dn730f $P/dn730f-c71a9987 bench bpftime_text_segment_transformer bpftime-cli-cpp; echo "Dn730f rc=$?"
chmod a-w $R/builds
echo "chain d done $(date +%T)"
