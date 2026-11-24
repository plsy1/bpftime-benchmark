#!/usr/bin/env bash
# Paper campaign, Jetson: one measurement session (adapted from the x86 session.sh).
# Usage: session.sh <session-id> <plan-file> <expected-minutes>
# The plan file holds one run_paper.py argument line per step (without --config, --session,
# --out, --capdir); lines starting with # are skipped.
# Guards: check_env.sh start/end (clocks, leftovers, shm, sha256, load, timers).
# This session's Claude process tree and the harness run on cpu2 (policy M keeps 0,1 for wrk,
# 3 for sslsniff, 4,5 for nginx); restored on exit. Container removed on exit.
set -euo pipefail
SID=${1:?session id}; PLAN=${2:?plan file}; MIN=${3:?expected minutes}
R=$HOME/src/paper-runs-20261124
H=$R/harness
OUT=$R/raw/$SID
IMAGE=bpftime-ssl-nginx:20261023
NAME=paper-jetson
mkdir "$OUT"
LOG=$OUT/log; mkdir "$LOG"
cp "$PLAN" "$OUT/plan.txt"
(cd "$H" && sha256sum run_paper.py nginx.conf config-jetson.json check_env.sh session.sh) >"$LOG/harness-sha256.txt"
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
    date --iso-8601=seconds >"$LOG/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$LOG/started-at.txt"

"$H/check_env.sh" start "$LOG" "$MIN"
{ uname -r; cat /proc/cmdline; grep -r . /sys/devices/system/cpu/vulnerabilities/; } >"$LOG/kernel.txt"
sudo -n docker image inspect -f '{{.Id}}' "$IMAGE" >"$LOG/image-id.txt"
cmp -s "$LOG/image-id.txt" "$R/image-id.txt" || { echo "image id changed"; exit 1; }

# Move this Claude session's process tree to cpu2.
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

thermal() { for z in /sys/class/thermal/thermal_zone*; do printf "%s=%s " "$(cat $z/type)" "$(cat $z/temp 2>/dev/null)"; done; echo; }
step=0
while IFS= read -r line; do
    case $line in ''|'#'*) continue;; esac
    step=$((step + 1))
    echo "step $step start $(date +%T) $(thermal)" >>"$LOG/steps.txt"
    # shellcheck disable=SC2086
    # PAPER_EXEC_PATH (E7): prepend the host perf dir; nsenter -t 1 -m resolves perf with this PATH in the host mount ns
    sudo -n timeout 6000 docker exec ${PAPER_EXEC_PATH:+-e "PATH=$PAPER_EXEC_PATH"} "$NAME" taskset -c 2 python3 /bench/run_paper.py \
        --config /bench/config-jetson.json --session "$SID" --out /out/rows.jsonl --capdir /out/cap \
        $line 2>&1 | tee -a "$OUT/console.log"
    echo "step $step end   $(date +%T) $(thermal)" >>"$LOG/steps.txt"
done <"$PLAN"
