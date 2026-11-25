#!/usr/bin/env bash
# C1: smoke first (must exit 0 with all rows valid), then the C1 session. Retries only on "load too high".
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; H=$R/harness; P=$R/plans
waitload() { until awk '{exit !($1 < 0.10)}' /proc/loadavg; do sleep 5; done; }
run() { local id=$1 plan=$2; for i in 1 2 3; do waitload; "$H/session.sh" "$id-try$i" "$plan" >"$R/logs/$id-try$i.out" 2>&1; s=$?; echo "$id try$i exit $s"
  [ $s -eq 0 ] && { LAST=$R/raw/$id-try$i; return 0; }; grep -q "load too high" "$R/logs/$id-try$i.out" || return $s; done; return 1; }
D=$(date +%Y%m%d)
run "C1smoke-$D" "$P/C1-smoke.txt" || { echo SMOKE-FAILED; exit 1; }
bad=$(python3 -c "import json,sys; r=[json.loads(l) for l in open('$LAST/rows.jsonl')]; print(sum(not x['valid'] for x in r), len(r))")
echo "smoke invalid/total: $bad"; [ "${bad%% *}" = 0 ] || { echo SMOKE-INVALID; exit 1; }
run "C1-$D" "$P/C1.txt"; echo "C1 exit $?"; echo C1-DONE
