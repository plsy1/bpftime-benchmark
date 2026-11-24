#!/usr/bin/env bash
# E2 pinbench, loaded state (Jetson), same structure as the x86 PBload step (NUC session, 2026-11-23):
# one 60 s B leg (policy M: nginx {4,5}, wrk {0,1}) in the background, pinbench starts 10 s later on the
# sslsniff core (cpu3, idle under B) with 10 runs x 200000 iterations and the same masks as the idle run.
# pinbench takes ~31 s at 200k iterations, inside the leg's load window.
set -euo pipefail
SID=${1:?session id}
R=$HOME/src/paper-runs-20261124; H=$R/harness; OUT=$R/raw/$SID; LOG=$OUT/log
IMAGE=bpftime-ssl-nginx:20261023; NAME=paper-jetson
mkdir "$OUT" "$LOG"
CLAUDE_PIDS=()
finish()
{
    status=$?
    trap - EXIT
    sudo -n docker rm -f "$NAME" >>"$LOG/cleanup.log" 2>&1 || true
    for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 0-5 "$p" >/dev/null 2>&1 || true; done
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT" "$H" 2>/dev/null || true
    "$H/check_env.sh" end "$LOG" >>"$LOG/check-end.txt" 2>&1 || { echo "end guard FAILED" >>"$LOG/warnings.txt"; status=98; }
    printf '%s\n' "$status" >"$LOG/exit-status.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
"$H/check_env.sh" start "$LOG" 10
(cd "$R/bench-extra" && sha256sum --check --quiet sha256.txt)
(cd "$H" && sha256sum run_paper.py nginx.conf config-jetson.json) >"$LOG/harness-sha256.txt"
p=$$
while [ "$p" -gt 1 ]; do
    c=$(cat /proc/$p/comm 2>/dev/null || true)
    case $c in claude|node|2.1.*|ccd-cli*) CLAUDE_PIDS+=("$p");; esac
    p=$(awk '{print $4}' /proc/$p/stat)
done
for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 2 "$p" >>"$LOG/claude-taskset.txt" 2>&1 || true; done
taskset -pc 2 $$ >/dev/null
sudo -n docker run -d --name "$NAME" --privileged --pid=host --shm-size=1g -v /sys:/sys \
    -v /lib/modules:/lib/modules:ro -v "$H":/bench -v "$R/builds":/builds:ro \
    -v "$R/sslsniff":/sslsniff:ro -v "$OUT":/out "$IMAGE" sleep infinity >"$LOG/container-id.txt"
sudo -n timeout 300 docker exec "$NAME" taskset -c 2 python3 /bench/run_paper.py --config /bench/config-jetson.json \
    --session "$SID" --exp PBload --out /out/rows.jsonl --capdir /out/cap --payload 16 --reps 1 --secs 60 --legs B \
    >"$OUT/pbload-leg.log" 2>&1 &
leg=$!
sleep 10
date --iso-8601=seconds >"$OUT/pinbench-started-at.txt"
timeout 120 taskset -c 3 "$R/bench-extra/pinbench" 3 1728 10 200000 single=3 pair=3,2 xpair=3,4 clusterA=0-3 all=0-5 \
    >"$OUT/pinbench-loaded.jsonl" 2>"$OUT/pinbench-loaded.stderr"
date --iso-8601=seconds >"$OUT/pinbench-finished-at.txt"
wait "$leg"
cat "$OUT/pbload-leg.log"
